#include <sys/param.h>
#include <sys/types.h>

#include <x86/sev.h> 

static struct sev_ops *g_sev_ops = NULL;

int
hook_sev_ops(struct sev_ops *ops)
{
	if (g_sev_ops != NULL)
		return (EEXIST);
	g_sev_ops = ops;
	return (0);
}

void
unhook_sev_ops(void)
{
	g_sev_ops = NULL;
}

int
sevops_asp_wbinvd(void)
{
	if (g_sev_ops == NULL || g_sev_ops->asp_wbinvd == NULL)
		return (ENODEV);
	return g_sev_ops->asp_wbinvd();
}

int
sevops_platform_init(uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->platform_shutdown == NULL)
		return (ENODEV);
	return g_sev_ops->platform_init(asp_error);
}

int
sevops_platform_shutdown(uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->platform_shutdown == NULL)
		return (ENODEV);
	return g_sev_ops->platform_shutdown(asp_error);
}

int
sevops_platform_status(struct sev_platform_status *pstatus, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->platform_status == NULL)
		return (ENODEV);
	return g_sev_ops->platform_status(pstatus, asp_error);
}

int
sevops_guest_launch_start(struct sev_launch_start *glaunch_start, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_start == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_start(glaunch_start, asp_error);
}

int
sevops_guest_activate(struct sev_activate *gactivate, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_activate == NULL)
		return (ENODEV);
	return g_sev_ops->guest_activate(gactivate, asp_error);
}

int
sevops_guest_status(struct sev_guest_status *gstatus, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_status == NULL)
		return (ENODEV);
	return g_sev_ops->guest_status(gstatus, asp_error);
}

int
sevops_guest_launch_update_data(struct sev_launch_update_data *glaunch_update_data, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_update_data == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_update_data(glaunch_update_data, asp_error);
}

// LAUNCH_UPDATE_DATA is for SEV-ES, is still work in progress
int
sevops_guest_launch_update_vmsa(struct sev_launch_update_vmsa *glaunch_update_vmsa, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_update_vmsa == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_update_vmsa(glaunch_update_vmsa, asp_error);
}

int
sevops_guest_launch_finish(struct sev_launch_finish *glaunch_finish, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_finish == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_finish(glaunch_finish, asp_error);
}

int
sevops_guest_launch_measure(struct sev_launch_measure *glmeasure, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_measure == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_measure(glmeasure, asp_error);
}

int
sevops_guest_launch_secret(struct sev_launch_secret *glsecret, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_secret == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_secret(glsecret, asp_error);
}

int
sevops_guest_shutdown(struct sev_guest_shutdown_args *args, uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_shutdown == NULL)
		return (ENODEV);
	return g_sev_ops->guest_shutdown(args, asp_error);
}

int
sevops_df_flush(uint32_t *asp_error)
{
	if (g_sev_ops == NULL || g_sev_ops->df_flush == NULL)
		return (ENODEV);
	return g_sev_ops->df_flush(asp_error);
}
