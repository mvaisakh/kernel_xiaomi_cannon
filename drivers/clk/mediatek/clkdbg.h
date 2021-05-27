/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

struct seq_file;

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_setl(addr, val)	clk_writel(addr, clk_readl(addr) | (val))
#define clk_clrl(addr, val)	clk_writel(addr, clk_readl(addr) & ~(val))

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN,
	ABIST_2,
};

struct fmeter_clk {
	enum FMETER_TYPE type;
	u32 id;
	const char *name;
	u32 ofs;
	u32 pdn;
};

struct regbase {
	u32 phys;
	void __iomem *virt;
	const char *name;
	const char *pg;
};

struct regname {
	struct regbase *base;
	u32 ofs;
	const char *name;
};

#define ADDR(rn)	(rn->base->virt + rn->ofs)
#define PHYSADDR(rn)	(rn->base->phys + rn->ofs)

struct cmd_fn {
	const char	*cmd;
	int (*fn)(struct seq_file *s, void *v);
};

#define CMDFN(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

struct provider_clk {
	const char *provider_name;
	u32 idx;
	struct clk *ck;
	u32 pwr_mask;
};

struct clkdbg_ops {
	const struct fmeter_clk *(*get_all_fmeter_clks)(void);
	void *(*prepare_fmeter)(void);
	void (*unprepare_fmeter)(void *data);
	u32 (*fmeter_freq)(const struct fmeter_clk *fclk);
	const struct regname *(*get_all_regnames)(void);
	const char * const *(*get_all_clk_names)(void);
	const char * const *(*get_pwr_names)(void);
	void (*setup_provider_clk)(struct provider_clk *pvdck);
	u32 (*get_spm_pwr_status)(void);
	bool (*is_pwr_on)(struct provider_clk *pvdck);
};

void set_clkdbg_ops(const struct clkdbg_ops *ops);
void set_custom_cmds(const struct cmd_fn *cmds);

struct provider_clk *get_all_provider_clks(void);
const char *get_last_cmd(void);

void reg_pdrv(const char *pdname);
void unreg_pdrv(const char *pdname);
void prepare_enable_provider(const char *pvd);
void disable_unprepare_provider(const char *pvd);

void print_regs(void);
void print_fmeter_all(void);
