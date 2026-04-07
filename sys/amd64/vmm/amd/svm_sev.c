#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/bitstring.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/vmm/vmm_mem.h>
#include <dev/vmm/vmm_vm.h>

#include <machine/vmm.h>

#include <x86/sev.h>

#include "x86.h"
#include "vmcb.h"
#include "svm.h"
#include "svm_softc.h"

/* Guest policy */
#define NODBG	(1ULL << 0) 
#define NOKS	(1ULL << 1)
#define ES		(1ULL << 2)
#define NOSEND	(1ULL << 3)
#define DOMAIN	(1ULL << 4)
#define SEV 	(1ULL << 5)

/* AMD SEV ASID parameters */
static struct mtx sev_asid_mtx;
static bitstr_t *sev_asid_bitmap;
static uint32_t sev_asid_min;
static uint32_t sev_asid_max;
static uint8_t sev_hardware_supported = false;

int
svm_sev_hardware_init(void)
{
	u_int regs[4];

	if (sev_hardware_supported)
		return (0);

	do_cpuid(0x80000000, regs);
	if (regs[0] < 0x8000001f) {
		printf("SVM: SEV not supported by CPU\n");
		return (EINVAL);
	}

	/* CPUID Fn8000_001F is for AMD SEV feature */
	do_cpuid(0x8000001f, regs);
	if ((regs[0] & 0x02) == 0) {
		printf("SVM: SEV feature is not enabled\n");
		return (EINVAL);
	}

	sev_asid_max = regs[2];
	sev_asid_min = regs[3];

	if (sev_asid_max < sev_asid_min || sev_asid_max == 0) {
		printf("SVM: Invalid SEV ASID range (min: %u, max: %u)\n", sev_asid_min, sev_asid_max);
		return (ENOMEM);
	}

	if (sevops_platform_init() != 0) {
		printf("SEV: Failed to initialize ASP SEV hardware platform\n");
		return (EINVAL);
	}

	mtx_init(&sev_asid_mtx, "sev_asid", NULL, MTX_DEF);
	sev_asid_bitmap = bit_alloc(sev_asid_max, M_DEVBUF, M_WAITOK | M_ZERO);

	sev_hardware_supported = true;
	printf("SVM: SEV Hardware Initialized. ASID range: %u - %u\n", sev_asid_min, sev_asid_max);

	return (0);
}

void
svm_sev_hardware_free(void)
{
	if (!sev_hardware_supported)
		return;

	free(sev_asid_bitmap, M_DEVBUF);
	mtx_destroy(&sev_asid_mtx);
	sev_hardware_supported = false;
}

static int
svm_sev_alloc_asid(void)
{
	if (!sev_hardware_supported)
		return -1;

	int bit;
	uint32_t asid = 0;

	mtx_lock(&sev_asid_mtx);

	bit_ffc(sev_asid_bitmap, sev_asid_max, &bit);
	if (bit >= 0) {
		bit_set(sev_asid_bitmap, bit);
		asid = sev_asid_min + bit;
	}

	mtx_unlock(&sev_asid_mtx);

	if (bit < 0){
		printf("%s: No free ASID available\n", __func__);
		return (ENOMEM);
	}

	return (asid);
}

static void
svm_sev_free_asid(uint32_t asid)
{
	if (!sev_hardware_supported)
		return;

	if (asid < sev_asid_min || asid >= (sev_asid_min + sev_asid_max))
		return;
	int bit = asid - sev_asid_min;

	mtx_lock(&sev_asid_mtx);

	bit_clear(sev_asid_bitmap, bit);

	mtx_unlock(&sev_asid_mtx);
}

static int
svm_sev_activate(struct svm_softc *sc)
{
	struct sev_activate g_activate;

	if (sevops_df_flush()) {
		printf("%s: failed to df flush\n", __func__);
		return (EINVAL);
	}

	bzero(&g_activate, sizeof(g_activate));
	g_activate.handle = sc->handle;
	g_activate.asid = sc->sev_asid;
	if (sevops_guest_activate(&g_activate)) {
		printf("%s: failed to activate sev asid\n", __func__);
		return (EINVAL);
	}

	return (0);
}

int
svm_sev_launch_start(struct svm_softc *sc)
{
	struct sev_platform_status	p_status;
	struct sev_launch_start		g_ls;
	struct sev_guest_status		g_status;
	int allocated_asid;
	u_int regs[4];
	
	sc->sev_enable = true;
	allocated_asid = svm_sev_alloc_asid();
	if (allocated_asid <= 0) {
		printf("%s: failed to allocate SEV ASID\n", __func__);
		return (ENOMEM);
	}

	/* CPUID Fn8000_001F is for AMD SEV feature */
	do_cpuid(0x8000001f, regs);
	if ((regs[0] & 0x02) == 0) {
		printf("SVM: SEV feature is not enabled\n");
		return (EINVAL);
	}

	sc->sev_asid = allocated_asid;
	sc->sev_c_bit = regs[1] & 0x3f; // EBX[5:0]

	bzero(&g_ls, sizeof(g_ls));
	/* Currently disable SEV-ES */
	g_ls.policy = (NODBG | NOKS | NOSEND | DOMAIN | SEV);
	if (sevops_guest_launch_start(&g_ls) != 0) {
		printf("%s: failed to launch start\n", __func__);
		svm_sev_free_asid(sc->sev_asid);
		return (EINVAL);
	}
	sc->handle = g_ls.handle;

	bzero(&g_status, sizeof(g_status));
	g_status.handle = g_ls.handle;
	if (sevops_guest_status(&g_status) != 0) {
		printf("%s: failed to get sev guest status\n", __func__);
		return (EINVAL);
	}

	if (svm_sev_activate(sc) != 0) {
		printf("%s: failed to bind SEV ASID: %d with handle: %d\n", __func__, sc->sev_asid, sc->handle);
		return (EINVAL);
	}
	printf("%s: Successfully bouond SEV ASID: %d with handle: %d\n", __func__, sc->sev_asid, sc->handle);

	bzero(&p_status, sizeof(p_status));
	if (sevops_platform_status(&p_status) != 0) {
		printf("%s: failed to get sev platform status\n", __func__);
		return (EINVAL);
	}
	printf("SVM: SEV API version: %d.%d\n", p_status.api_major, p_status.api_minor);
	printf("SVM: State: %d\n", p_status.state);
	printf("SVM: Guests: %d\n", p_status.guest_count);

	return (0);
}

int
svm_sev_launch_update_data(struct svm_softc *sc, struct sev_launch_update_data_vm *udata)
{
	vm_paddr_t gpa;
	size_t size, offset, len;
	void *kva, *cookie;
	int error;
	struct sev_launch_update_data g_ludata;

	gpa = udata->vaddr;
	size = udata->length;
	// gpa_end = gpa + size;

	bzero(&g_ludata, sizeof(g_ludata));
	g_ludata.handle = sc->handle;

	sevops_asp_wbinvd();

	for(offset = 0; offset < size;offset += len) {
		len = MIN(PAGE_SIZE - ((gpa + offset) & PAGE_MASK), size - offset);

		/* wire the virtual memory */
		kva = vm_gpa_hold_global(sc->vm, gpa + offset, len, VM_PROT_READ | VM_PROT_WRITE, &cookie);
		if (kva == NULL) {
			printf("%s: failed to hold GPA 0x%lx (size: %lu)\n", __func__, gpa, size);
			return (EFAULT);
		}

		g_ludata.paddr = vtophys(kva);
		g_ludata.length = len;

		/* for debug */
		/*
		if (out < 200) {
			printf("[*] UPDATE_DATA GPA: 0x%lx -> KVA: %p -> HPA: 0x%lx\n",
					gpa + offset, kva, g_ludata.paddr);
			printf("[*] Memory Content (Before) : ");
			for (int i = 0; i < 16; i++) {
				printf("%02x ", ((unsigned char *)kva)[i]);
			}
			printf("\n");
		}
		*/
		/* for debug */

		error = sevops_guest_launch_update_data(&g_ludata);
		if (error) {
			printf("%s: ASP hw failed at GPA 0x%lx\n", __func__, gpa + offset);
			vm_gpa_release(cookie);
			break;
		}

		/* for debug */
		/*
		if (out < 200) {
			printf("[*] Memory Content (After) : ");
			for (int i = 0; i < 16; i++) {
				printf("%02x ", ((unsigned char *)kva)[i]);
			}
			printf("\n");
		}
		*/
		/* for debug */

		/* unwire the virtual memory */
		vm_gpa_release(cookie);
	}


	return (0);
}

int
svm_sev_launch_measure(struct svm_softc *sc, struct sev_launch_measure *lmeasure)
{
	int error;

	bzero(lmeasure, sizeof(*lmeasure));
	lmeasure->handle = sc->handle;
	lmeasure->measure_len = sizeof(lmeasure->measure) + sizeof(lmeasure->measure_nonce);

	error = sevops_guest_launch_measure(lmeasure);
	if (error) {
		printf("%s: failed to get measurement from hardware\n", __func__);
	}

	return (error);
}

int
svm_sev_launch_finish(struct svm_softc *sc)
{
	struct sev_launch_finish g_lf;

	bzero(&g_lf, sizeof(g_lf));
	g_lf.handle = sc->handle;
	if (sevops_guest_launch_finish(&g_lf)) {
		printf("%s: failed to launch finish\n", __func__);
		return (EINVAL);
	}

	return (0);
}

int
svm_sev_guest_shutdown(struct svm_softc *sc)
{
	struct sev_guest_shutdown_args g_shutdown;
	
	bzero(&g_shutdown, sizeof(g_shutdown));
	g_shutdown.handle = sc->handle;
	if (sevops_guest_shutdown(&g_shutdown)) {
		printf("%s: failed to shutdown guest sev\n", __func__);
		return (EINVAL);
	}
	if (sc->sev_asid != 0) {
		svm_sev_free_asid(sc->sev_asid);
		sc->sev_asid = 0;
	}
	sc->handle = 0;
	sc->sev_enable = false;

	return (0);
}
