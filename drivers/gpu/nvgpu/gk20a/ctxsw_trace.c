/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/uaccess.h>
#include <linux/poll.h>

#include "ctxsw_trace.h"

int gk20a_ctxsw_dev_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int gk20a_ctxsw_dev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

long gk20a_ctxsw_dev_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	return ENOTTY;
}
