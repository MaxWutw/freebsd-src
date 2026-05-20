/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Anish Gupta (akgupt3@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SVM_H_
#define _SVM_H_

struct pcpu;
struct svm_softc;
struct svm_vcpu;
struct sev_user_platform_status;
struct sev_user_launch_start;
struct sev_launch_update_data_vm;
struct sev_launch_measure;
struct sev_user_launch_secret;

/*
 * Guest register state that is saved outside the VMCB.
 */
struct svm_regctx {
	register_t	sctx_rbp;
	register_t	sctx_rbx;
	register_t	sctx_rcx;
	register_t	sctx_rdx;
	register_t	sctx_rdi;
	register_t	sctx_rsi;
	register_t	sctx_r8;
	register_t	sctx_r9;
	register_t	sctx_r10;
	register_t	sctx_r11;
	register_t	sctx_r12;
	register_t	sctx_r13;
	register_t	sctx_r14;
	register_t	sctx_r15;
	register_t	sctx_dr0;
	register_t	sctx_dr1;
	register_t	sctx_dr2;
	register_t	sctx_dr3;

	register_t	host_dr0;
	register_t	host_dr1;
	register_t	host_dr2;
	register_t	host_dr3;
	register_t	host_dr6;
	register_t	host_dr7;
	uint64_t	host_debugctl;
};

void svm_launch(uint64_t pa, struct svm_regctx *gctx, struct pcpu *pcpu);
#ifdef BHYVE_SNAPSHOT
void svm_set_tsc_offset(struct svm_vcpu *vcpu, uint64_t offset);
#endif

/* 
 * AMD SEV functions 
 */
struct svm_softc;
struct vm_sev_cmd;

int svm_sev_hardware_init(void);
void svm_sev_hardware_free(void);

void svm_sev_free_asid(uint32_t asid);

int svm_sev_platform_status(struct svm_softc *sc, struct sev_user_platform_status *upstatus, uint32_t *asp_error);
int svm_sev_launch_start(struct svm_softc *sc, uint32_t *asp_error);
int svm_sev_launch_start_with_session(struct svm_softc *sc, struct sev_user_launch_start *uls, uint32_t *asp_error);
int svm_sev_launch_update_data(struct svm_softc *sc, struct sev_launch_update_data_vm *udata, uint32_t *asp_error);
int svm_sev_launch_measure(struct svm_softc *sc, struct sev_launch_measure *lmeasure, uint32_t *asp_error);
int svm_sev_launch_secret(struct svm_softc *sc, struct sev_user_launch_secret *lsecret, uint32_t *asp_error);
int svm_sev_launch_finish(struct svm_softc *sc, uint32_t *asp_error);
int svm_sev_guest_shutdown(struct svm_softc *sc, uint32_t *asp_error);

#endif /* _SVM_H_ */
