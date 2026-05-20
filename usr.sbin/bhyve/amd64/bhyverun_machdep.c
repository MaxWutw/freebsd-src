/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sysexits.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include <vmmapi.h>

#include <x86/sev.h>

#include "acpi.h"
#include "atkbdc.h"
#include "bhyverun.h"
#include "bootrom.h"
#include "config.h"
#include "debug.h"
#include "e820.h"
#include "fwctl.h"
#include "ioapic.h"
#include "inout.h"
#include "kernemu_dev.h"
#include "mptbl.h"
#include "pci_emul.h"
#include "pci_irq.h"
#include "pci_lpc.h"
#include "rtc.h"
#include "smbiostbl.h"
#include "xmsr.h"

void
bhyve_init_config(void)
{
	init_config();

	/* Set default values prior to option parsing. */
	set_config_bool("acpi_tables", true);
	set_config_bool("acpi_tables_in_memory", true);
	set_config_value("memory.size", "256M");
	set_config_bool("x86.strictmsr", true);
	set_config_bool("x86.verbosemsr", false);
	set_config_value("lpc.fwcfg", "bhyve");
}

void
bhyve_usage(int code)
{
	const char *progname;

	progname = getprogname();

	fprintf(stderr,
	    "Usage: %s [-aCDeHhPSuWwxY]\n"
	    "       %*s [-c [[cpus=]numcpus][,sockets=n][,cores=n][,threads=n]]\n"
	    "       %*s [-G port] [-k config_file] [-l lpc] [-m mem] [-o var=value]\n"
	    "       %*s [-p vcpu:hostcpu] [-r file] [-s pci] [-U uuid] vmname\n"
	    "       -a: local apic is in xAPIC mode (deprecated)\n"
	    "       -C: include guest memory in core file\n"
	    "       -c: number of CPUs and/or topology specification\n"
	    "       -D: destroy on power-off\n"
	    "       -e: exit on unhandled I/O access\n"
	    "       -G: start a debug server\n"
	    "       -H: vmexit from the guest on HLT\n"
	    "       -h: help\n"
	    "       -k: key=value flat config file\n"
	    "       -K: PS2 keyboard layout\n"
	    "       -l: LPC device configuration\n"
	    "       -M: monitor mode\n"
	    "       -m: memory size\n"
	    "       -n: NUMA domain specification\n"
	    "       -o: set config 'var' to 'value'\n"
	    "       -P: vmexit from the guest on pause\n"
	    "       -p: pin 'vcpu' to 'hostcpu'\n"
#ifdef BHYVE_SNAPSHOT
	    "       -r: path to checkpoint file\n"
#endif
	    "       -S: guest memory cannot be swapped\n"
	    "       -s: <slot,driver,configinfo> PCI slot config\n"
	    "       -U: UUID\n"
	    "       -u: RTC keeps UTC time\n"
	    "       -W: force virtio to use single-vector MSI\n"
	    "       -w: ignore unimplemented MSRs\n"
	    "       -x: local APIC is in x2APIC mode\n"
	    "       -Y: disable MPtable generation\n",
	    progname, (int)strlen(progname), "", (int)strlen(progname), "",
	    (int)strlen(progname), "");
	exit(code);
}

void
bhyve_optparse(int argc, char **argv)
{
	const char *optstr;
	int c;

#ifdef BHYVE_SNAPSHOT
	optstr = "aehuwxACDHIMPSWYk:f:o:p:G:c:s:m:n:l:K:U:r:";
#else
	optstr = "aehuwxACDHIMPSWYk:f:o:p:G:c:s:m:n:l:K:U:";
#endif
	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
		case 'a':
			set_config_bool("x86.x2apic", false);
			break;
		case 'A':
			/*
			 * NOP. For backward compatibility. Most systems don't
			 * work properly without sane ACPI tables. Therefore,
			 * we're always generating them.
			 */
			break;
		case 'D':
			set_config_bool("destroy_on_poweroff", true);
			break;
		case 'p':
			if (bhyve_pincpu_parse(optarg) != 0) {
				errx(EX_USAGE, "invalid vcpu pinning "
				    "configuration '%s'", optarg);
			}
			break;
		case 'c':
			if (bhyve_topology_parse(optarg) != 0) {
			    errx(EX_USAGE, "invalid cpu topology "
				"'%s'", optarg);
			}
			break;
		case 'C':
			set_config_bool("memory.guest_in_core", true);
			break;
		case 'f':
			if (qemu_fwcfg_parse_cmdline_arg(optarg) != 0) {
				errx(EX_USAGE, "invalid fwcfg item '%s'",
				    optarg);
			}
			break;
		case 'G':
			bhyve_parse_gdb_options(optarg);
			break;
		case 'k':
			bhyve_parse_simple_config_file(optarg);
			break;
		case 'K':
			set_config_value("keyboard.layout", optarg);
			break;
		case 'l':
			if (strncmp(optarg, "help", strlen(optarg)) == 0) {
				lpc_print_supported_devices();
				exit(0);
			} else if (lpc_device_parse(optarg) != 0) {
				errx(EX_USAGE, "invalid lpc device "
				    "configuration '%s'", optarg);
			}
			break;
#ifdef BHYVE_SNAPSHOT
		case 'r':
			restore_file = optarg;
			break;
#endif
		case 's':
			if (strncmp(optarg, "help", strlen(optarg)) == 0) {
				pci_print_supported_devices();
				exit(0);
			} else if (pci_parse_slot(optarg) != 0)
				exit(BHYVE_EXIT_ERROR);
			else
				break;
		case 'S':
			set_config_bool("memory.wired", true);
			break;
		case 'm':
			set_config_value("memory.size", optarg);
			break;
		case 'M':
			set_config_bool("monitor", true);
			break;
		case 'n':
			if (bhyve_numa_parse(optarg) != 0)
				errx(EX_USAGE,
				    "invalid NUMA configuration "
				    "'%s'",
				    optarg);
			if (!get_config_bool("acpi_tables"))
				errx(EX_USAGE, "NUMA emulation requires ACPI");
			break;
		case 'o':
			if (!bhyve_parse_config_option(optarg)) {
				errx(EX_USAGE,
				    "invalid configuration option '%s'",
				    optarg);
			}
			break;
		case 'H':
			set_config_bool("x86.vmexit_on_hlt", true);
			break;
		case 'I':
			/*
			 * The "-I" option was used to add an ioapic to the
			 * virtual machine.
			 *
			 * An ioapic is now provided unconditionally for each
			 * virtual machine and this option is now deprecated.
			 */
			break;
		case 'P':
			set_config_bool("x86.vmexit_on_pause", true);
			break;
		case 'e':
			set_config_bool("x86.strictio", true);
			break;
		case 'u':
			set_config_bool("rtc.use_localtime", false);
			break;
		case 'U':
			set_config_value("uuid", optarg);
			break;
		case 'w':
			set_config_bool("x86.strictmsr", false);
			break;
		case 'W':
			set_config_bool("virtio_msix", false);
			break;
		case 'x':
			set_config_bool("x86.x2apic", true);
			break;
		case 'Y':
			set_config_bool("x86.mptable", false);
			break;
		case 'h':
			bhyve_usage(0);
		default:
			bhyve_usage(1);
		}
	}

	/* Handle backwards compatibility aliases in config options. */
	if (get_config_value("lpc.bootrom") != NULL &&
	    get_config_value("bootrom") == NULL) {
		warnx("lpc.bootrom is deprecated, use '-o bootrom' instead");
		set_config_value("bootrom", get_config_value("lpc.bootrom"));
	}
	if (get_config_value("lpc.bootvars") != NULL &&
	    get_config_value("bootvars") == NULL) {
		warnx("lpc.bootvars is deprecated, use '-o bootvars' instead");
		set_config_value("bootvars", get_config_value("lpc.bootvars"));
	}
}

void
bhyve_init_vcpu(struct vcpu *vcpu)
{
	int err, tmp;

	if (get_config_bool_default("x86.vmexit_on_hlt", false)) {
		err = vm_get_capability(vcpu, VM_CAP_HALT_EXIT, &tmp);
		if (err < 0) {
			EPRINTLN("VM exit on HLT not supported");
			exit(BHYVE_EXIT_ERROR);
		}
		vm_set_capability(vcpu, VM_CAP_HALT_EXIT, 1);
	}

	if (get_config_bool_default("x86.vmexit_on_pause", false)) {
		/*
		 * pause exit support required for this mode
		 */
		err = vm_get_capability(vcpu, VM_CAP_PAUSE_EXIT, &tmp);
		if (err < 0) {
			EPRINTLN("SMP mux requested, no pause support");
			exit(BHYVE_EXIT_ERROR);
		}
		vm_set_capability(vcpu, VM_CAP_PAUSE_EXIT, 1);
	}

	if (get_config_bool_default("x86.x2apic", false))
		err = vm_set_x2apic_state(vcpu, X2APIC_ENABLED);
	else
		err = vm_set_x2apic_state(vcpu, X2APIC_DISABLED);

	if (err) {
		EPRINTLN("Unable to set x2apic state (%d)", err);
		exit(BHYVE_EXIT_ERROR);
	}

	vm_set_capability(vcpu, VM_CAP_ENABLE_INVPCID, 1);

	err = vm_set_capability(vcpu, VM_CAP_IPI_EXIT, 1);
	assert(err == 0);
}

void
bhyve_start_vcpu(struct vcpu *vcpu, bool bsp)
{
	int error;

	if (bsp) {
		if (bootrom_boot()) {
			error = vm_set_capability(vcpu,
			    VM_CAP_UNRESTRICTED_GUEST, 1);
			if (error != 0) {
				err(4, "ROM boot failed: unrestricted guest "
				    "capability not available");
			}
			error = vcpu_reset(vcpu);
			assert(error == 0);
		}
	} else {
		bhyve_init_vcpu(vcpu);

		/*
		 * Enable the 'unrestricted guest' mode for APs.
		 *
		 * APs startup in power-on 16-bit mode.
		 */
		error = vm_set_capability(vcpu, VM_CAP_UNRESTRICTED_GUEST, 1);
		assert(error == 0);
	}

	fbsdrun_addcpu(vcpu_id(vcpu));
}

static int
sev_attestation_pause(struct vmctx *ctx, struct sev_launch_measure *sev_measure,
					  struct sev_user_platform_status *sev_status, uint32_t sev_policy)
{
	const char *vm_name;
	char sock_path[4096];
	struct sockaddr_un addr;
	int server_fd, client_fd;
	unsigned char blob[48];
	char *b64_str;
	long b64_len;
	char measure_b64[256];
	BIO *b64, *mem;
	int done = 0;
	/* int error = 0; */

	vm_name = get_config_value("name");
	if (vm_name == NULL)
		vm_name = "default";

	snprintf(sock_path, sizeof(sock_path), "/var/run/bhyve_sev_%s.sock", vm_name);
	unlink(sock_path);

	server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_fd < 0) {
		warnx("SEV socket failed");
		return (-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, sock_path, sizeof(addr.sun_path));

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		warnx("SEV socket bind failed: %s", sock_path);
		close(server_fd);
		return (-1);
	}

	if (listen(server_fd, 1) < 0) {
		warnx("SEV socket listen failed");
		close(server_fd);
		unlink(sock_path);
		return (-1);
	}

	memcpy(blob, sev_measure->measure, 32);
	memcpy(blob + 32, sev_measure->measure_nonce, 16);

	b64 = BIO_new(BIO_f_base64());
	mem = BIO_new(BIO_s_mem());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	b64 = BIO_push(b64, mem);
	BIO_write(b64, blob, 48);
	BIO_flush(b64);
	b64_len = BIO_get_mem_data(mem, &b64_str);

	snprintf(measure_b64, sizeof(measure_b64), "%.*s", (int)b64_len, b64_str);
	BIO_free_all(b64);

	printf("==== AMD SEV Attestation pause ====\n");

	while (!done) {
		FILE *client_fp;
		char line[4096];

		client_fd = accept(server_fd, NULL, NULL);
		if (client_fd < 0) {
			warnx("SEV accept client failed");
			continue;
		}

		client_fp = fdopen(client_fd, "r+");
		if (client_fp == NULL) {
			close(client_fd);
			continue;
		}

		fprintf(client_fp, "sev_status=paused\n");
		fprintf(client_fp, "commands=query, secret, continue\n\n");
		fprintf(client_fp, "$ ");
		fflush(client_fp);

		while (fgets(line, sizeof(line), client_fp) != NULL) {
			size_t len = strlen(line);
			while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';

            if (strcmp(line, "query") == 0) {
                fprintf(client_fp, "api_major=%u\n", sev_status->api_major);
                fprintf(client_fp, "api_minor=%u\n", sev_status->api_minor);
                fprintf(client_fp, "build_id=%u\n", (sev_status->cfges_build >> 24) & 0xFF);
                fprintf(client_fp, "policy=0x%x\n", sev_policy);
                /* fprintf(client_fp, "handle=%u\n", handle); */
                fprintf(client_fp, "state=launch-secret\n");
                fprintf(client_fp, "launch_measure_blob=%s\n", measure_b64);
                fprintf(client_fp, "\n");
                fflush(client_fp);
			} else if (strncmp(line, "secret ", 7) == 0) {
				/* 
				 * Format: secret <header_b64> <payload_b64> [gpa]
				 * header and payload are base64-encoded form
				 */
				char hdr_b64[4096], payload_b64[8192], gpa_str[32];
				uint64_t gpa;	
				int nargs;
				struct sev_user_launch_secret ulsecret;
				struct vm_sev_cmd sevcmd;
				void *hdr_buf = NULL, *payload_buf = NULL;
				int hdr_size = 0, payload_size = 0;

				nargs = sscanf(line + 7, "%4095s %8191s %31s", hdr_b64, payload_b64, gpa_str);
				if (nargs < 2) {
					fprintf(client_fp, "error=usage: secret <header_b64> <payload_b64> [gpa]\n\n");
					fprintf(client_fp, "$ ");
					fflush(client_fp);
					continue;
				}

				if (nargs >= 3 && strlen(gpa_str) > 0) {
					gpa = strtoull(gpa_str, NULL, 0);
				} else {
					/* 
					 * If the gpa is not given by user, we currently use 0x810000 to be the hardcoded secret addresss.
					 * The secret address 0x810000 was written in edk2 AmdSev OVMF.
					 */
					gpa = 0x810000;
				}

				/* Decode header from b64 */
				{
					BIO *b64d = BIO_new(BIO_f_base64());
					BIO *memd = BIO_new_mem_buf(hdr_b64, strlen(hdr_b64));
					BIO_set_flags(b64d, BIO_FLAGS_BASE64_NO_NL);
					memd = BIO_push(b64d, memd);
					hdr_buf = malloc(strlen(hdr_b64));
					if (hdr_buf != NULL)
						hdr_size = BIO_read(memd, hdr_buf, strlen(hdr_b64));
					BIO_free_all(memd);
				}
				if (hdr_buf == NULL || hdr_size < 0) {
					fprintf(client_fp, "error=failed to decode header base64\n");
					fflush(client_fp);
					free(hdr_buf);
					continue;
				}

				/* Decode payload from b64 */
				{
					BIO *b64d = BIO_new(BIO_f_base64());
					BIO *memd = BIO_new_mem_buf(payload_b64, strlen(payload_b64));
					BIO_set_flags(b64d, BIO_FLAGS_BASE64_NO_NL);
					memd = BIO_push(b64d, memd);
					payload_buf = malloc(strlen(payload_b64));
					if (payload_buf != NULL)
						payload_size = BIO_read(memd, payload_buf, strlen(payload_b64));
					BIO_free_all(memd);
				}
				if (payload_buf == NULL || payload_size < 0) {
					fprintf(client_fp, "error=failed to decode payload base64\n");
					fflush(client_fp);
					free(payload_buf);
					continue;
				}
				
				printf("Injecting secret: header=%d bytes, payload=%d bytes, GPA=0x%lx\n",
						hdr_size, payload_size, (unsigned long)gpa);
				
				bzero(&ulsecret, sizeof(ulsecret));
				ulsecret.hdr_vaddr = (uint64_t)hdr_buf;
				ulsecret.hdr_length = (uint32_t)hdr_size;
				ulsecret.guest_gpa = gpa;
				ulsecret.guest_length = (uint32_t)payload_size;
				ulsecret.trans_vaddr = (uint64_t)payload_buf;
				ulsecret.trans_length = (uint32_t)payload_size;

				bzero(&sevcmd, sizeof(sevcmd));
				sevcmd.cmd = VM_SEV_CMD_LAUNCH_SECRET;
				sevcmd.data = &ulsecret;
				sevcmd.error = 0;
				if (vm_sev_command(ctx, &sevcmd) < 0) {
					fprintf(client_fp, "error=LAUNCH_SECRET failed (errno=%d, asp_error=0x%zx)\n\n",
							errno, sevcmd.error);
				}
				else {
					fprintf(client_fp, "ok=secret injected (%d bytes at GPA 0x%lx)\n\n", 
							payload_size, (unsigned long)gpa);
				}
				fflush(client_fp);

				free(hdr_buf);
				free(payload_buf);

			} else if (strcmp(line, "continue") == 0) {
				fprintf(client_fp, "ok=continuing boot\n\n");
				fflush(client_fp);
				done = 1;
				break;
			} else {
				fprintf(client_fp, "error=unknown command\n");
				fprintf(client_fp, "commands=query, secret, continue\n\n");
				fflush(client_fp);
			}
			fprintf(client_fp, "$ ");
			fflush(client_fp);
		}
		fclose(client_fp);
	}

	close(server_fd);
	unlink(sock_path);
	return (0);
}

int
bhyve_init_platform(struct vmctx *ctx, struct vcpu *bsp __unused)
{
	int error;
	struct sev_launch_measure measure;
	struct vm_sev_cmd sevcmd;
	struct sev_user_platform_status sev_status;
	const char *sev_policy_str;
	uint32_t sev_policy;

	error = init_msr();
	if (error != 0)
		return (error);
	init_inout();
	kernemu_dev_init();
	atkbdc_init(ctx);
	pci_irq_init(ctx);
	ioapic_init(ctx);
	rtc_init(ctx);
	sci_init(ctx);
	error = e820_init(ctx);
	if (error != 0)
		return (error);
	error = bootrom_loadrom(ctx);
	if (error != 0)
		return (error);

	if (get_config_bool_default("amd.sev.enable", false)) {
		bzero(&measure, sizeof(measure));
		measure.measure_len = 48;

		/* SEV LAUNCH_MEASURE */
		bzero(&sevcmd, sizeof(sevcmd));
		sevcmd.cmd = VM_SEV_CMD_LAUNCH_MEASURE;
		sevcmd.data = &measure;
		sevcmd.error = 0;
		if (vm_sev_command(ctx, &sevcmd) < 0)
			errx(EX_OSERR, "Failed to MEASURE SEV guest");

		/* Attestation pause with socket */
		if (get_config_bool_default("amd.sev.pause", false)) {
			sev_policy_str = get_config_value("amd.sev.policy");
			// If the sev_policy is not provided, we use 0x3b directly.
			sev_policy = sev_policy_str ? (uint32_t)strtoul(sev_policy_str, NULL, 0) : 0x3b;

			bzero(&sev_status, sizeof(sev_status));
			bzero(&sevcmd, sizeof(sevcmd));
			sevcmd.cmd = VM_SEV_CMD_PLATFORM_STATUS;
			sevcmd.data = &sev_status;
			sevcmd.error = 0;
			
			if (vm_sev_command(ctx, &sevcmd) < 0)
				errx(EX_OSERR, "Failed to get SEV platform status");

			sev_attestation_pause(ctx, &measure, &sev_status, sev_policy);
		}

		/* SEV LAUNCH_FINISH */
		bzero(&sevcmd, sizeof(sevcmd));
		sevcmd.cmd = VM_SEV_CMD_LAUNCH_FINISH;
		sevcmd.data = NULL;
		sevcmd.error = 0;
		if (vm_sev_command(ctx, &sevcmd) < 0)
			errx(EX_OSERR, "Failed to FINISH SEV guest");
	}

	return (0);
}

int
bhyve_init_platform_late(struct vmctx *ctx, struct vcpu *bsp __unused)
{
	int error;

	if (get_config_bool_default("x86.mptable", true)) {
		error = mptable_build(ctx, guest_ncpus);
		if (error != 0)
			return (error);
	}
	error = smbios_build(ctx);
	if (error != 0)
		return (error);
	error = e820_finalize();
	if (error != 0)
		return (error);

	if (bootrom_boot() && strcmp(lpc_fwcfg(), "bhyve") == 0)
		fwctl_init();

	if (get_config_bool("acpi_tables")) {
		error = acpi_build(ctx, guest_ncpus);
		assert(error == 0);
	}

	return (0);
}
