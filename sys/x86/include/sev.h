#ifndef _MACHINE_SEV_H_
#define _MACHINE_SEV_H_

#define DECLARE_SEVOPS_FUNC(ret_type, cmd, args)	\
	typedef ret_type (*sevops_##cmd##_t) args;		\
	ret_type sevops_##cmd##_t args

DECLARE_SEVOPS_FUNC(int, platform_init, (struct asp_softc *));
DECLARE_SEVOPS_FUNC(int, platform_shutdown, (struct asp_softc *));
DECLARE_SEVOPS_FUNC(int, platform_status, (struct asp_softc *, struct sev_platform_status *));
DECLARE_SEVOPS_FUNC(int, guest_launch_start, (struct asp_softc *, struct sev_launch_start *));
DECLARE_SEVOPS_FUNC(int, guest_status, (struct asp_softc *, struct sev_guest_status *));
DECLARE_SEVOPS_FUNC(int, guest_launch_update_data, (struct asp_softc *, struct sev_launch_update_data *));
DECLARE_SEVOPS_FUNC(int, guest_launch_update_vmsa, (struct asp_softc *, struct sev_launch_update_vmsa *));
DECLARE_SEVOPS_FUNC(int, guest_launch_finish, (struct asp_softc *, struct sev_launch_finish *));
DECLARE_SEVOPS_FUNC(int, guest_activate, (struct asp_softc *, struct sev_activate *));
DECLARE_SEVOPS_FUNC(int, guest_shutdown, (struct asp_softc *, struct sev_guest_shutdown_args *));

struct sev_ops {
	/* platform sev ops */
	sevops_platform_init_t 		platform_init;
	sevops_platform_shutdown_t 	platform_shutdown;
	sevope_platform_status_t 	platform_status;

	/* guest sev ops */
	sevops_guest_launch_start_t 		guest_launch_start;
	sevops_guest_status_t 				guest_status;
	sevops_guest_launch_update_data_t 	guest_launch_update_data;
	sevops_guest_launch_update_vmsa_t	guest_launch_update_vmsa;
	sevops_guest_launch_finish_t		guest_launch_finish;
	sevops_guest_activate_t				guest_activate;
	sevops_guest_shutdown_t				guest_shutdown;
};

#endif
