#pragma once

/* AMD Family 19h */
/* PSP hardware */
#define PSP_REG_INTEN 		0x10690
#define PSP_REG_INTSTS		0x10694
/* ASP Mailbox register */
#define ASP_REG_CMDRESP 	0x10980
#define ASP_REG_ADDR_LO 	0x109e0
#define ASP_REG_ADDR_HI		0x109e4

/* SEV API Version 0.24 */
/* Platform commands */
#define SEV_CMD_INIT				0x01	
#define SEV_CMD_SHUTDOWN			0x02
#define SEV_CMD_PLATFORM_RESET		0x03		
#define SEV_CMD_PLATFORM_STATUS		0x04	
#define SEV_CMD_PEK_GEN				0x05
#define SEV_CMD_PEK_CSR				0x06
#define SEV_CMD_PEK_CERT_IMPORT 	0x07
#define SEV_CMD_PDH_CERT_EXPORT		0x08
#define SEV_CMD_PDH_GEN				0x09
#define SEV_CMD_DF_FLUSH			0x0a
#define SEV_CMD_DOWNLOAD_FIRMWARE	0x0b

/* Guest commands */
#define SEV_CMD_DECOMMISSION 		0x20
#define SEV_CMD_ACTIVATE			0x21
#define SEV_CMD_DEACTIVATE			0x22
#define SEV_CMD_GUEST_STATUS		0x23
#define SEV_CMD_LAUNCH_START		0x30
#define SEV_CMD_LAUNCH_UPDATE_DATA 	0x31
#define SEV_CMD_LAUNCH_UPDATE_VMSA	0x32
#define SEV_CMD_LAUNCH_MEASURE		0x33
#define SEV_CMD_LAUNCH_UPDATE_SECRET 0x34
#define SEV_CMD_LAUNCH_FINISH		0x35
#define SEV_CMD_ATTESTATION			0x36

#define ASP_CMDRESP_RESPONSE (1 << 31)
#define ASP_CMDRESP_COMPLETE (1 << 1)
#define ASP_CMDRESP_IOC		 (1 << 0)

/* Status Code */
#define SEV_STATUS_MASK				0xffff
#define SEV_STATUS_SUCCESS			0x0000

#define SEV_TMR_SIZE (1024 * 1024)

struct asp_softc {
	device_t dev;
	bool detaching;
	int32_t cmd_size;

	/* Primary BAR (RID 2) used for register access */
	struct resource 	*pci_resource;
	int32_t 			pci_resource_id;
	bus_space_tag_t 	pci_bus_tag;
	bus_space_handle_t 	pci_bus_handle;

	/* Secondary BAR (RID 5) apparently used for MSI-X */
	int	pci_resource_id_msix;
	struct resource *pci_resource_msix;
	int	 irq_rid;
	struct resource *irq_res;
	void *irq_tag;

	/* ASP register */
	bus_size_t		reg_cmdresp;
	bus_size_t		reg_addr_lo;
	bus_size_t		reg_addr_hi;
	bus_size_t		reg_inten;
	bus_size_t		reg_intsts;

	/* DMA Resources */
	bus_dma_tag_t 	parent_dma_tag;
	bus_dma_tag_t 	cmd_dma_tag;
	bus_dmamap_t 	cmd_dma_map;
	void			*cmd_kva;
	bus_addr_t		cmd_paddr;

	/* TMR Resources, currently disabled */
	/*
	size_t			tmr_size;
	bus_dma_tag_t 	tmr_dma_tag;
	bus_dmamap_t 	tmr_dma_map;
	void			*tmr_kva;
	bus_addr_t		tmr_paddr;
	*/

	struct mtx mtx_lock;
};
