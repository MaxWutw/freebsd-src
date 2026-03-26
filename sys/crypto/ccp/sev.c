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
sevops_platform_init(void)
{
	if (g_sev_ops == NULL || g_sev_ops->platform_shutdown == NULL)
		return (ENODEV);
	return g_sev_ops->platform_init();
}

int
sevops_platform_shutdown(void)
{
	if (g_sev_ops == NULL || g_sev_ops->platform_shutdown == NULL)
		return (ENODEV);
	return g_sev_ops->platform_shutdown();
}

int
sevops_platform_status(struct sev_platform_status *pstatus)
{
	if (g_sev_ops == NULL || g_sev_ops->platform_status == NULL)
		return (ENODEV);
	return g_sev_ops->platform_status(pstatus);
}

int
sevops_guest_launch_start(struct sev_launch_start *glaunch_start)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_start == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_start(glaunch_start);
}

int
sevops_guest_activate(struct sev_activate *gactivate)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_activate == NULL)
		return (ENODEV);
	return g_sev_ops->guest_activate(gactivate);
}

int
sevops_guest_status(struct sev_guest_status *gstatus)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_status == NULL)
		return (ENODEV);
	return g_sev_ops->guest_status(gstatus);
}

int
sevops_guest_launch_update_data(struct sev_launch_update_data *glaunch_update_data)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_update_data == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_update_data(glaunch_update_data);
}

int
sevops_guest_launch_update_vmsa(struct sev_launch_update_vmsa *glaunch_update_vmsa)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_update_vmsa == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_update_vmsa(glaunch_update_vmsa);
}

int
sevops_guest_launch_finish(struct sev_launch_finish *glaunch_finish)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_finish == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_finish(glaunch_finish);
}

int
sevops_guest_launch_measure(struct sev_launch_measure *glmeasure)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_launch_measure == NULL)
		return (ENODEV);
	return g_sev_ops->guest_launch_measure(glmeasure);
}

int
sevops_guest_shutdown(struct sev_guest_shutdown_args *args)
{
	if (g_sev_ops == NULL || g_sev_ops->guest_shutdown == NULL)
		return (ENODEV);
	return g_sev_ops->guest_shutdown(args);
}

int
sevops_df_flush(void)
{
	if (g_sev_ops == NULL || g_sev_ops->df_flush == NULL)
		return (ENODEV);
	return g_sev_ops->df_flush();
}
