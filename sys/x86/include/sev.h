#ifndef _MACHINE_SEV_H_
#define _MACHINE_SEV_H_

#define DECLARE_SEVOPS_FUNC(ret_type, cmd, args)	\
	typedef ret_type (*sevops_##cmd##_t) args;		\
	ret_type sevops_##cmd args

struct sev_platform_status;
struct sev_ops;

DECLARE_SEVOPS_FUNC(int, platform_init, (void));
DECLARE_SEVOPS_FUNC(int, platform_shutdown, (void));
DECLARE_SEVOPS_FUNC(int, platform_status, (struct sev_platform_status *));
/*
DECLARE_SEVOPS_FUNC(int, guest_launch_start, (struct asp_softc *, struct sev_launch_start *));
DECLARE_SEVOPS_FUNC(int, guest_status, (struct asp_softc *, struct sev_guest_status *));
DECLARE_SEVOPS_FUNC(int, guest_launch_update_data, (struct asp_softc *, struct sev_launch_update_data *));
DECLARE_SEVOPS_FUNC(int, guest_launch_update_vmsa, (struct asp_softc *, struct sev_launch_update_vmsa *));
DECLARE_SEVOPS_FUNC(int, guest_launch_finish, (struct asp_softc *, struct sev_launch_finish *));
DECLARE_SEVOPS_FUNC(int, guest_activate, (struct asp_softc *, struct sev_activate *));
DECLARE_SEVOPS_FUNC(int, guest_shutdown, (struct asp_softc *, struct sev_guest_shutdown_args *));
*/

int hook_sev_ops(struct sev_ops *ops);
void unhook_sev_ops(void);

struct sev_init {
	uint32_t enable_es; 	/* In */
	uint32_t reserved;		/* - */
	uint64_t tmr_paddr;		/* In */
	uint32_t tmr_length;	/* In */
} __packed;

struct sev_platform_status {
	uint8_t 	api_major;		/* Out */
	uint8_t 	api_minor;		/* Out */
	uint8_t 	state;			/* Out */
	uint8_t 	owner;			/* Out */
	uint32_t 	cfges_build;	/* Out */
	uint32_t 	guest_count;	/* Out */
} __packed;

struct sev_activate {
	uint32_t handle;	/* In */
	uint32_t asid;		/* In */
} __packed;

struct sev_deactivate {
	uint32_t handle;	/* In */
} __packed;

struct sev_decommission {
	uint32_t handle;	/* In */
} __packed;

struct sev_guest_status {
	uint32_t handle;	/* In  */
	uint32_t policy;	/* Out */
	uint32_t asid;		/* Out */
	uint8_t  state;		/* Out */
} __packed;

struct sev_launch_start {
	uint32_t handle;		/* In/Out */
	uint32_t policy;		/* In */
	uint64_t dh_cert_paddr; /* In */
	uint32_t dh_cert_len;	/* In */
	uint32_t reserved;		/* - */
	uint64_t session_paddr; /* In */
	uint32_t session_len;	/* In */
} __packed;

struct sev_launch_update_data {
	uint32_t handle;	/* In */
	uint32_t reserved;	/* - */
	uint64_t paddr;		/* In */
	uint32_t length;	/* In */
} __packed;

struct sev_launch_update_vmsa {
	uint32_t handle;	/* In */
	uint32_t reserved;	/* - */
	uint64_t paddr;		/* In */
	uint32_t length;	/* In */
} __packed;

struct sev_launch_measure {
	uint32_t handle;			/* In */
	uint32_t reserved;			/* - */
	uint64_t measure_paddr;		/* In */
	uint32_t measure_len;		/* In/Out */
	uint8_t measure[32];		/* Out */
	uint8_t measure_nonce[16];  /* Out */
} __packed;

struct sev_launch_finish {
	uint32_t handle;	/* In */
} __packed;

struct sev_attestation {
	uint32_t handle;		/* In */
	uint32_t reserved;		/* - */
	uint64_t paddr;			/* In */
	uint8_t mnounce[16];	/* In */
	uint32_t length;		/* In/Out */
} __packed;

struct sev_guest_shutdown_args {
	uint32_t handle;
} __packed;

struct sev_ops {
	/* platform sev ops */
	sevops_platform_init_t 		platform_init;
	sevops_platform_shutdown_t 	platform_shutdown;
	sevops_platform_status_t 	platform_status;

	/* guest sev ops 
	sevops_guest_launch_start_t 		guest_launch_start;
	sevops_guest_status_t 				guest_status;
	sevops_guest_launch_update_data_t 	guest_launch_update_data;
	sevops_guest_launch_update_vmsa_t	guest_launch_update_vmsa;
	sevops_guest_launch_finish_t		guest_launch_finish;
	sevops_guest_activate_t				guest_activate;
	sevops_guest_shutdown_t				guest_shutdown;
	*/
};

#endif /* _MACHINE_SEV_H_ */
