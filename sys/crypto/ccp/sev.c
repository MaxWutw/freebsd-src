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
sevops_platform_status(struct sev_platform_status *status)
{
	if (g_sev_ops == NULL || g_sev_ops->platform_status == NULL)
		return (ENODEV);
	return g_sev_ops->platform_status(status);
}
