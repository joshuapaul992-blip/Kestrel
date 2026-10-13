/* include/nvfw/sec2.h -- Kestrel's Linux KPI: SEC2 firmware argument and message layouts; written here because nouveau's copy has no license notice */
#ifndef _LKPI_NV_INCLUDE_NVFW_SEC2_H
#define _LKPI_NV_INCLUDE_NVFW_SEC2_H

struct nv_sec2_args {
	u32 freq_hz;
	u32 falc_trace_size;
	u32 falc_trace_dma_base;
	u32 falc_trace_dma_idx;
	bool secure_mode;
};

/* unit ids (original and v2 firmware numbering) */
#define NV_SEC2_UNIT_INIT	0x01
#define NV_SEC2_UNIT_UNLOAD	0x06
#define NV_SEC2_UNIT_ACR	0x08

#define NV_SEC2_UNIT_V2_INIT	0x01
#define NV_SEC2_UNIT_V2_UNLOAD	0x05
#define NV_SEC2_UNIT_V2_ACR	0x07

/* INIT unit */
#define NV_SEC2_INIT_MSG_INIT			0x00
#define NV_SEC2_INIT_MSG_QUEUE_ID_CMDQ		0x00
#define NV_SEC2_INIT_MSG_QUEUE_ID_MSGQ		0x01

struct nv_sec2_init_msg {
	struct nvfw_falcon_msg hdr;
	u8 msg_type;
	u8 num_queues;
	u16 os_debug_entry_point;
	struct {
		u32 offset;
		u16 size;
		u8 index;
		u8 id;
	} queue_info[2];
	u32 sw_managed_area_offset;
	u16 sw_managed_area_size;
};

struct nv_sec2_init_msg_v1 {
	struct nvfw_falcon_msg hdr;
	u8 msg_type;
	u8 num_queues;
	u16 os_debug_entry_point;
	struct {
		u32 offset;
		u16 size;
		u8 index;
		u8 id;
	} queue_info[2];
	u32 sw_managed_area_offset;
	u16 sw_managed_area_size;
	u32 unkn[8];
};

/* ACR unit */
#define NV_SEC2_ACR_CMD_BOOTSTRAP_FALCON		0x00
#define NV_SEC2_ACR_BOOTSTRAP_FALCON_FLAGS_RESET_YES	0x00000000
#define NV_SEC2_ACR_BOOTSTRAP_FALCON_FLAGS_RESET_NO	0x00000001

struct nv_sec2_acr_cmd {
	struct nvfw_falcon_cmd hdr;
	u8 cmd_type;
};

struct nv_sec2_acr_msg {
	struct nvfw_falcon_cmd hdr;
	u8 msg_type;
};

struct nv_sec2_acr_bootstrap_falcon_cmd {
	struct nv_sec2_acr_cmd cmd;
	u32 flags;
	u32 falcon_id;
};

struct nv_sec2_acr_bootstrap_falcon_msg {
	struct nv_sec2_acr_msg msg;
	u32 error_code;
	u32 falcon_id;
};

struct nv_sec2_acr_bootstrap_falcon_cmd_v1 {
	struct nv_sec2_acr_cmd cmd;
	u32 flags;
	u32 falcon_id;
	u32 unkn08;
	u32 unkn0c;
};

struct nv_sec2_acr_bootstrap_falcon_msg_v1 {
	struct nv_sec2_acr_msg msg;
	u32 error_code;
	u32 falcon_id;
	u32 unkn08;
};

#endif /* _LKPI_NV_INCLUDE_NVFW_SEC2_H */
