/*
    $Id: vcd_image_vpkt.h,v 1.2 2001/09/20 20:03:52 hvr Exp $

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>
    Copyright (C) 2001 Arnd Bergmann <arnd@itreff.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __VCD_IMAGE_VPKT_H__
#define __VCD_IMAGE_VPKT_H__

#include <libvcd/vcd_recorder.h>
#include <libvcd/vcd_image.h>

VcdImageSink *vcd_image_sink_new_vpkt (VcdRecorder * recorder);

#endif /* __VCD_IMAGE_DAO_H__ */
