#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <x86/sev.h>

#include "asp.h"

struct sev_ops *sevops = NULL;
static struct asp_softc *g_asp_softc = NULL;

struct pciid {
	uint32_t devid;
	const char *desc;
} asp_ids[] = {
	{ 0x14861022, "AMD Secure Processor (Milan)" },
	{ 0x00000000, NULL }
};
static struct sev_ops asp_sev_ops_impl;
static int asp_detach(device_t dev);
/* static int asp_hw_platform_init(struct asp_softc *sc); */
static int asp_hw_platform_shutdown(struct asp_softc *sc);
static int asp_hw_platform_status(struct asp_softc *sc, struct sev_platform_status *pstatus);
/*
int sev_platform_init(struct asp_softc *sc);
int sev_platform_shutdown(struct asp_softc *sc);
int sev_platform_status(struct asp_softc *sc, struct sev_platform_status *pstatus);
int sev_guest_launch_start(struct asp_softc *sc, struct sev_launch_start *gstatus);
int sev_guest_status(struct asp_softc *sc, struct sev_guest_status *gstatus);
int sev_guest_launch_update_data(struct asp_softc *sc, struct sev_launch_update_data *gludata);
int sev_guest_launch_update_vmsa(struct asp_softc *sc, struct sev_launch_update_vmsa *gluvmsa);
int sev_guest_launch_finish(struct asp_softc *sc, struct sev_launch_finish *gfinish);
int sev_guest_activate(struct asp_softc *sc, struct sev_activate *gactivate);
int sev_guest_shutdown(struct asp_softc *sc, struct sev_guest_shutdown_args *args);
*/

static int
asp_probe(device_t dev)
{
	struct pciid *ip;
	uint32_t id;
	id = pci_get_devid(dev);
	
	for (ip = asp_ids; ip < &asp_ids[nitems(asp_ids)]; ip++) {
		if (id == ip->devid) {
			device_set_desc(dev, ip->desc);
			return 0;
		}
	}
	return (ENXIO);
}

static int
asp_map_pci_bar(device_t dev)
{
	struct asp_softc *sc;
	sc = device_get_softc(dev);

	sc->pci_resource_id = PCIR_BAR(2);
	sc->pci_resource = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		&sc->pci_resource_id, RF_ACTIVE);
	if (sc->pci_resource == NULL) {
		device_printf(dev, "unable to allocate pci resource\n");
		return (ENODEV);
	}

	sc->pci_resource_id_msix = PCIR_BAR(5);
	sc->pci_resource_msix = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		&sc->pci_resource_id_msix, RF_ACTIVE);
	if (sc->pci_resource_msix == NULL) {
		device_printf(dev, "unable to allocate pci resource\n");
		return (ENODEV);
	}

	sc->pci_bus_tag = rman_get_bustag(sc->pci_resource);
	sc->pci_bus_handle = rman_get_bushandle(sc->pci_resource);
	return (0);
}

static int
asp_ummap_pci_bar(device_t dev)
{
	struct asp_softc *sc;

	sc = device_get_softc(dev);
	
	bus_release_resource(dev, SYS_RES_MEMORY, sc->pci_resource_id_msix,
		sc->pci_resource_msix);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->pci_resource_id,
		sc->pci_resource);
	
	return (0);
}

static void
asp_intr_handler(void *arg)
{
	struct asp_softc *sc = arg;
	uint32_t status;

	mtx_lock(&sc->mtx_lock);

	status = bus_read_4(sc->pci_resource, sc->reg_intsts);
	bus_write_4(sc->pci_resource, sc->reg_intsts, status);
	if (status & ASP_CMDRESP_COMPLETE) {
		wakeup(sc);
	}

	mtx_unlock(&sc->mtx_lock);
}

static void 
asp_dma_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t*)arg = segs[0].ds_addr;
}

static int
asp_attach(device_t dev)
{
	struct asp_softc *sc;
	uint32_t val;
	int error;
	int msixc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* softc get base register */
	sc->reg_cmdresp = ASP_REG_CMDRESP;
	sc->reg_addr_lo = ASP_REG_ADDR_LO;
	sc->reg_addr_hi = ASP_REG_ADDR_HI;
	sc->reg_inten	= PSP_REG_INTEN;
	sc->reg_intsts 	= PSP_REG_INTSTS;

	mtx_init(&sc->mtx_lock, "asp", NULL, MTX_DEF);

	error = asp_map_pci_bar(dev);
	if (error != 0) {
		device_printf(sc->dev, "%s: Failed to map BAR(s)\n", __func__);
		goto fail;
	}

	error = pci_enable_busmaster(dev);
	if (error != 0) {
		device_printf(sc->dev, "%s: Failed to enable busmaster\n", __func__);
		goto fail;
	}

	/* Setup MSI-X */
	msixc = 1;
	error = pci_alloc_msix(sc->dev, &msixc);
	if (error != 0) {
		device_printf(sc->dev, "%s: Failed to alloccate IRQ resource\n", __func__);
		goto fail;
	}

	sc->irq_rid = 1;
	sc->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ, &sc->irq_rid, RF_ACTIVE | RF_SHAREABLE);
	error = bus_setup_intr(sc->dev, sc->irq_res,
			INTR_TYPE_MISC | INTR_MPSAFE, NULL, asp_intr_handler,
			sc, &sc->irq_tag);
	if (error != 0) {
		device_printf(sc->dev, "%s: Failed to setup interrupt handler\n", __func__);
		goto fail;
	}

	
	val = bus_read_4(sc->pci_resource, 0x00);
	device_printf(dev, "ASP Hardware found! Reg[0x00] = 0x%08x\n", val);

	device_printf(dev, "Memory mapped at physical address: 0x%lx\n", rman_get_start(sc->pci_resource));

	/* Allocate the parent DMA tag apporpriate for PCI */
	error = bus_dma_tag_create(bus_get_dma_tag(dev), 
			1,
			0,
			BUS_SPACE_MAXADDR,
			BUS_SPACE_MAXADDR,
			NULL, NULL,
			BUS_SPACE_MAXSIZE_32BIT,
			BUS_SPACE_UNRESTRICTED,
			BUS_SPACE_MAXSIZE_32BIT,
			0,
			NULL, NULL,
			&sc->parent_dma_tag);
	if (error != 0) {
		device_printf(sc->dev, "Failed to create parent DMA tag\n");
		goto fail;
	}

	/* Allocate DMA for command buffer */
	error = bus_dma_tag_create(sc->parent_dma_tag,
			PAGE_SIZE,
			0,
			BUS_SPACE_MAXADDR,
			BUS_SPACE_MAXADDR,
			NULL, NULL,
			PAGE_SIZE,
			1,
			PAGE_SIZE,
			0,
			NULL, NULL,
			&sc->cmd_dma_tag);
	if (error != 0) {
		device_printf(sc->dev, "Failed to create command DMA tag\n");
		goto fail;
	}
	error = bus_dmamem_alloc(sc->cmd_dma_tag, (void **)&sc->cmd_kva,
							BUS_DMA_WAITOK | BUS_DMA_ZERO, &sc->cmd_dma_map);
	if (error != 0) {
		device_printf(sc->dev, "Failed to allocate command DMA map\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->cmd_dma_tag, sc->cmd_dma_map, sc->cmd_kva,
							PAGE_SIZE, asp_dma_cb, &sc->cmd_paddr, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->dev, "Failed to load memory\n");
		goto fail;
	}

	/* Enable ASP interrupt */
	bus_write_4(sc->pci_resource, sc->reg_inten, -1);

	if (g_asp_softc == NULL)
		g_asp_softc = sc;

	hook_sev_ops(&asp_sev_ops_impl);

	/* error = asp_hw_platform_init(sc); */

	/* Test for get SEV platform status */
	/*
	struct sev_platform_status pstatus;
	error = asp_hw_platform_status(sc, &pstatus);
	if (error != 0) {
		device_printf(dev, "%s: Failed to get SEV platform status\n", __func__);
		goto fail;
	}
	device_printf(sc->dev, "SEV status:\n");
	device_printf(sc->dev, "	API version: %d.%d\n", pstatus.api_major, pstatus.api_minor);
	device_printf(sc->dev, "	State: %d\n", pstatus.state);
	device_printf(sc->dev, "	Guests: %d\n", pstatus.guest_count);

	asp_hw_platform_shutdown(sc);
	error = asp_hw_platform_status(sc, &pstatus);
	if (error != 0) {
		device_printf(dev, "%s: Failed to get SEV platform status\n", __func__);
		goto fail;
	}
	device_printf(sc->dev, "SEV status:\n");
	device_printf(sc->dev, "	API version: %d.%d\n", pstatus.api_major, pstatus.api_minor);
	device_printf(sc->dev, "	State: %d\n", pstatus.state);
	device_printf(sc->dev, "	Guests: %d\n", pstatus.guest_count);
	*/

fail:
	if (error != 0) {
		asp_detach(dev);
	}

	return (error);
}

static int
asp_detach(device_t dev)
{
	struct asp_softc *sc;

	sc = device_get_softc(dev);

	asp_hw_platform_shutdown(sc);

	bus_generic_detach(dev);
	pci_disable_busmaster(dev);
	if (sc->irq_tag != NULL)
		bus_teardown_intr(sc->dev, sc->irq_res, sc->irq_tag);

	if (sc->irq_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_IRQ, sc->irq_rid, sc->irq_res);

	pci_release_msi(sc->dev);

	asp_ummap_pci_bar(dev);

	if (sc->cmd_dma_map != 0)
		bus_dmamap_unload(sc->cmd_dma_tag, sc->cmd_dma_map);

	if (sc->cmd_dma_map != 0)
		bus_dmamem_free(sc->cmd_dma_tag, sc->cmd_kva, sc->cmd_dma_map);

	if (sc->cmd_dma_tag != 0)
		bus_dma_tag_destroy(sc->cmd_dma_tag);

	if (sc->parent_dma_tag != 0)
		bus_dma_tag_destroy(sc->parent_dma_tag);

	if (g_asp_softc == sc)
		g_asp_softc = NULL;

	unhook_sev_ops();

	mtx_destroy(&sc->mtx_lock);
	
	return (0);
}

static int
asp_wait(struct asp_softc *sc, uint32_t *status, int poll)
{
	int timeout = 1000, error;

	/* Polling */
	if (poll) {
		while (timeout > -1) {
			*status = bus_read_4(sc->pci_resource, sc->reg_cmdresp);
			if (*status & ASP_CMDRESP_RESPONSE)
				return (0);
			DELAY(5000);
			timeout--;
		}

		if (timeout == 0)
			return (ETIMEDOUT);
	}

	error = msleep(sc, &sc->mtx_lock, PWAIT, "asp", 2 * hz);
	if (error)
		return (error);

	*status = bus_read_4(sc->pci_resource, sc->reg_cmdresp);
	device_printf(sc->dev, "ASP Command finished. Result: 0x%x\n", *status);
	
	return (0);
}

static int
asp_send_cmd(struct asp_softc *sc, uint32_t cmd, uint64_t paddr)
{
	uint32_t paddr_hi, paddr_lo, cmd_id, status;
	int error;

	paddr_lo = paddr & 0xffffffff;
	paddr_hi = (paddr >> 32) & 0xffffffff;
	cmd_id = (cmd & 0x3ff) << 16;
	device_printf(sc->dev, "High address: 0x%x\n", paddr_hi);
	device_printf(sc->dev, "Low address: 0x%x\n", paddr_lo);

	/* nonzero if we are doing a cold boot */
	if (!cold)
		cmd_id |= ASP_CMDRESP_IOC;

	bus_write_4(sc->pci_resource, sc->reg_addr_lo, paddr_lo);
	bus_write_4(sc->pci_resource, sc->reg_addr_hi, paddr_hi);
	bus_write_4(sc->pci_resource, sc->reg_cmdresp, cmd_id);

	error = asp_wait(sc, &status, cold);

	if (error)
		return (error);

	if (status & ASP_CMDRESP_RESPONSE) {
		if ((status & SEV_STATUS_MASK) != SEV_STATUS_SUCCESS) {
			return (EIO);
		}
	}

	return (0);
}

static int
asp_hw_platform_init(struct asp_softc *sc)
{
	struct sev_init *init;
	int error;

	mtx_lock(&sc->mtx_lock);

	init = (struct sev_init *)sc->cmd_kva;
	if (init == NULL) {
		return (ENOMEM);
	}
	
	/* For AMD SEV, currently we does not enable SEV-ES */
	bzero(init, sizeof(struct sev_init));
	device_printf(sc->dev, "SEV init physical address: 0x%lx\n", sc->cmd_paddr);

	error = asp_send_cmd(sc, SEV_CMD_INIT, sc->cmd_paddr);

	mtx_unlock(&sc->mtx_lock);

	if (error)
		return (error);

	device_printf(sc->dev, "SEV init finished!\n");

	return (0);
}

static int
asp_hw_platform_shutdown(struct asp_softc *sc)
{
	int error;
	
	mtx_lock(&sc->mtx_lock);

	error = asp_send_cmd(sc, SEV_CMD_SHUTDOWN, 0x0);

	mtx_unlock(&sc->mtx_lock);


	return (error);
}

static int
asp_hw_platform_status(struct asp_softc *sc, struct sev_platform_status *pstatus)
{
	struct sev_platform_status *status_data;
	int error;

	mtx_lock(&sc->mtx_lock);

	status_data = (struct sev_platform_status*)sc->cmd_kva;
	bzero(status_data, sizeof(struct sev_platform_status));
	if (status_data == NULL) {
		return (ENOMEM);
	}

	error = asp_send_cmd(sc, SEV_CMD_PLATFORM_STATUS, sc->cmd_paddr);

	mtx_unlock(&sc->mtx_lock);

	if (error)
		return (error);

	bcopy(status_data, pstatus, sizeof(struct sev_platform_status));
	
	return (0);
}

static int
asp_hw_guest_launch_start(struct asp_softc *sc, struct sev_launch_start *glaunch_start)
{
	struct sev_launch_start *launch_start;
	int error;

	mtx_lock(&sc->mtx_lock);

	launch_start = (struct sev_launch_start*)sc->cmd_kva;
	bzero(launch_start, sizeof(struct sev_launch_start));

	launch_start->handle = glaunch_start->handle;
	launch_start->policy = glaunch_start->policy;

	error = asp_send_cmd(sc, SEV_CMD_LAUNCH_START, sc->cmd_paddr);

	mtx_unlock(&sc->mtx_lock);

	if (error)
		return (error);

	/* If the HANDLE field is zero, a new VEK is generated for this guest. */
	if (glaunch_start->handle == 0)
		glaunch_start->handle = launch_start->handle;

	return (0);
}

static int
asp_hw_guest_status(struct asp_softc *sc, struct sev_guest_status *gstatus)
{
	struct sev_guest_status *status;
	int error;

	mtx_lock(&sc->mtx_lock);

	status = (struct sev_guest_status*)sc->cmd_kva;
	bzero(status, sizeof(struct sev_guest_status));

	error = asp_send_cmd(sc, SEV_CMD_GUEST_STATUS, sc->cmd_paddr);
	
	mtx_unlock(&sc->mtx_lock);

	if (error)
		return (error);

	bcopy(status, gstatus, sizeof(struct sev_guest_status));

	return (0);
}

static int
asp_hw_guest_launch_update_data(struct asp_softc *sc, struct sev_launch_update_data *gludata)
{
	struct sev_launch_update_data *ludata;
	int error;

	mtx_lock(&sc->mtx_lock);

	ludata = (struct sev_launch_update_data*)sc->cmd_kva;
	bzero(ludata, sizeof(struct sev_launch_update_data));
	error = asp_send_cmd(sc, SEV_CMD_LAUNCH_UPDATE_DATA, sc->cmd_paddr);

	mtx_unlock(&sc->mtx_lock);

	return (error);
}

static int
asp_hw_guest_launch_update_vmsa(struct asp_softc *sc, struct sev_launch_update_vmsa *gluvmsa)
{
	struct sev_launch_update_vmsa *luvmsa;
	int error;

	mtx_lock(&sc->mtx_lock);

	luvmsa = (struct sev_launch_update_vmsa*)sc->cmd_kva;
	bzero(luvmsa, sizeof(struct sev_launch_update_vmsa));

	luvmsa->handle = gluvmsa->handle;
	luvmsa->paddr = gluvmsa->paddr;
	luvmsa->length = gluvmsa->length;

	error = asp_send_cmd(sc, SEV_CMD_LAUNCH_UPDATE_VMSA, sc->cmd_paddr);

	mtx_unlock(&sc->mtx_lock);

	return (error);
}

static int
asp_hw_guest_launch_finish(struct asp_softc *sc, struct sev_launch_finish *gfinish)
{
	struct sev_launch_finish *finish;
	int error;

	mtx_lock(&sc->mtx_lock);

	finish = (struct sev_launch_finish*)sc->cmd_kva;
	bzero(finish, sizeof(struct sev_launch_finish));
	
	finish->handle = gfinish->handle;
	
	error = asp_send_cmd(sc, SEV_CMD_LAUNCH_FINISH, sc->cmd_paddr);

	mtx_unlock(&sc->mtx_lock);

	return (error);
}

static int
asp_hw_guest_activate(struct asp_softc *sc, struct sev_activate *gactivate)
{
	struct sev_activate *activate;
	int error;

	mtx_lock(&sc->mtx_lock);

	activate = (struct sev_activate*)sc->cmd_kva;
	bzero(activate, sizeof(struct sev_activate));
	
	activate->handle = gactivate->handle;
	activate->asid = gactivate->asid;
	
	error = asp_send_cmd(sc, SEV_CMD_ACTIVATE, sc->cmd_paddr);

	mtx_unlock(&sc->mtx_lock);

	return (error);
}

static int
asp_hw_guest_df_flush(struct asp_softc *sc)
{
	int error;

	/* 
	 * The x86 system software must execute WBINVD 
	 * before invoking the DF_FLUSH command.	
	 */
	wbinvd();

	mtx_lock(&sc->mtx_lock);

	error = asp_send_cmd(sc, SEV_CMD_DF_FLUSH, 0x0);
	
	mtx_unlock(&sc->mtx_lock);

	return (error);
}

static int
asp_hw_guest_launch_measure(struct asp_softc *sc)
{
	/* struct sev_launch_measure *lm; */
	return (0);
}

static int
asp_hw_guest_deactivate(struct asp_softc *sc, struct sev_deactivate *gdeactivate)
{
	struct sev_deactivate *deactivate;
	int error;

	mtx_lock(&sc->mtx_lock);

	deactivate = (struct sev_deactivate*)sc->cmd_kva;
	bzero(deactivate, sizeof(struct sev_deactivate));
	
	deactivate->handle = gdeactivate->handle;
	
	error = asp_send_cmd(sc, SEV_CMD_DEACTIVATE, sc->cmd_paddr);
	mtx_unlock(&sc->mtx_lock);


	return (error);
}

static int
asp_hw_guest_decommission(struct asp_softc *sc, struct sev_decommission *arg)
{
	struct sev_decommission *decom;
	int error;

	mtx_lock(&sc->mtx_lock);

	decom = (struct sev_decommission*)sc->cmd_kva;
	bzero(decom, sizeof(struct sev_decommission));

	decom->handle = arg->handle;

	error = asp_send_cmd(sc, SEV_CMD_DECOMMISSION, sc->cmd_paddr);

	mtx_unlock(&sc->mtx_lock);

	return (error);
}

static int
asp_hw_guest_shutdown(struct asp_softc *sc, struct sev_guest_shutdown_args *args)
{
	struct sev_deactivate deactivate;
	struct sev_decommission decom;
	int error;
	
	bzero(&deactivate, sizeof(struct sev_deactivate));
	deactivate.handle = args->handle;
	error = asp_hw_guest_deactivate(sc, &deactivate);
	if (error)
		return  (error);

	error = asp_hw_guest_df_flush(sc);
	if (error)
		return  (error);

	bzero(&decom, sizeof(struct sev_decommission));
	decom.handle = args->handle;
	error = asp_hw_guest_decommission(sc, &decom);
	if (error)
		return  (error);

	return (0);
}

static int
sev_platform_init(void)
{
	if (g_asp_softc == NULL)
		return (ENXIO);

	return asp_hw_platform_init(g_asp_softc);
}

static int
sev_platform_shutdown(void)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_platform_shutdown(g_asp_softc);
}

static int
sev_platform_status(struct sev_platform_status *pstatus)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_platform_status(g_asp_softc, pstatus);
}

static int
sev_guest_launch_start(struct sev_launch_start *glaunch_start)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_guest_launch_start(g_asp_softc, glaunch_start);
}

static int
sev_guest_activate(struct sev_activate *gactivate)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_guest_activate(g_asp_softc, gactivate);
}

static int
sev_guest_status(struct sev_guest_status *gstatus)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_guest_status(g_asp_softc, gstatus);
}

static int
sev_guest_launch_update_data(struct sev_launch_update_data *glaunch_update_data)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_guest_launch_update_data(g_asp_softc, glaunch_update_data);
}

static int
sev_guest_launch_update_vmsa(struct sev_launch_update_vmsa *glaunch_update_vmsa)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_guest_launch_update_vmsa(g_asp_softc, glaunch_update_vmsa);
}

static int
sev_guest_launch_finish(struct sev_launch_finish *glaunch_finish)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_guest_launch_finish(g_asp_softc, glaunch_finish);
}

static int
sev_guest_shutdown(struct sev_guest_shutdown_args *args)
{
	if (g_asp_softc == NULL)
		return (ENXIO);
	return asp_hw_guest_shutdown(g_asp_softc, args);
}

static device_method_t asp_methods[] = {
	DEVMETHOD(device_probe, asp_probe),
	DEVMETHOD(device_attach, asp_attach),
	DEVMETHOD(device_detach, asp_detach),

	DEVMETHOD_END
};

static driver_t asp_driver = {
	"asp",
	asp_methods,
	sizeof(struct asp_softc)
};

static struct sev_ops asp_sev_ops_impl = {
	.platform_init = sev_platform_init,
	.platform_shutdown = sev_platform_shutdown,
	.platform_status = sev_platform_status,
	.guest_launch_start = sev_guest_launch_start,
	.guest_activate = sev_guest_activate,
	.guest_status = sev_guest_status,
	.guest_launch_update_data = sev_guest_launch_update_data,
	.guest_launch_update_vmsa = sev_guest_launch_update_vmsa,
	.guest_launch_finish = sev_guest_launch_finish,
	.guest_shutdown = sev_guest_shutdown
};

DRIVER_MODULE(asp, pci, asp_driver, NULL, NULL);
MODULE_VERSION(asp, 1);
MODULE_DEPEND(asp, pci, 1, 1, 1);
