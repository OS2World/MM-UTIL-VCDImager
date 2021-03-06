/*
    $Id: vcd_pbc.c,v 1.25 2001/08/22 23:14:37 hvr Exp $

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <libvcd/vcd_assert.h>
#include <stddef.h>
#include <math.h>

#include <libvcd/vcd_util.h>
#include <libvcd/vcd_pbc.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_obj.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_bytesex.h>

static const char _rcsid[] = "$Id: vcd_pbc.c,v 1.25 2001/08/22 23:14:37 hvr Exp $";

static uint8_t
_wtime (int seconds)
{
  if (seconds < 0)
    return 255;

  if (seconds <= 60)
    return seconds;

  if (seconds <= 2000)
    {
      double _tmp;

      _tmp = seconds;
      _tmp -= 60;
      _tmp /= 10;
      _tmp += 60;

      return rint (_tmp);
    }

  vcd_warn ("wait time of %ds clipped to 2000s", seconds);

  return 254;
}

static pbc_t *
_vcd_pbc_byid(const VcdObj *obj, const char item_id[])
{
  VcdListNode *node;

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      if (_pbc->id && !strcmp (item_id, _pbc->id))
	return _pbc;
    }

  /* not found */
  return NULL;
}

unsigned
_vcd_pbc_lid_lookup (const VcdObj *obj, const char item_id[])
{
  VcdListNode *node;
  unsigned n = 1;

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      vcd_assert (n < 0x8000);

      if (_pbc->id && !strcmp (item_id, _pbc->id))
	return n;

      n++;
    }

  /* not found */
  return 0;
}

static void
_set_area_helper (pbc_area_t *dest, const pbc_area_t *src, const char sel_id[])
{
  memset (dest, 0, sizeof (pbc_area_t));
  
  if (src)
    {
      if (src->x1 || src->x2 || src->y1 || src->y2) /* not disabled */
	{
	  if (src->x1 >= src->x2)
	    vcd_error ("selection '%s': area x1 >= x2 (%d >= %d)",
		       sel_id, src->x1, src->x2);

	  if (src->y1 >= src->y2)
	    vcd_error ("selection '%s': area y1 >= y2 (%d >= %d)",
		       sel_id, src->y1, src->y2);
	}
  
      *dest = *src;
    }
}

enum item_type_t
_vcd_pbc_lookup (const VcdObj *obj, const char item_id[])
{
  unsigned id;

  vcd_assert (item_id != NULL);

  if ((id = _vcd_pbc_pin_lookup (obj, item_id)))
    {
      if (id < 2)
	return ITEM_TYPE_NOTFOUND;
      else if (id < 100)
	return ITEM_TYPE_TRACK;
      else if (id < 600)
	return ITEM_TYPE_ENTRY;
      else if (id < 2980)
	return ITEM_TYPE_SEGMENT;
      else 
	vcd_assert_not_reached ();
    }
  else if (_vcd_pbc_lid_lookup (obj, item_id))
    return ITEM_TYPE_PBC;

  return ITEM_TYPE_NOTFOUND;
}

uint16_t
_vcd_pbc_pin_lookup (const VcdObj *obj, const char item_id[])
{
  int n;
  VcdListNode *node;

  if (!item_id)
    return 0;

  /* check sequence items */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *_sequence = _vcd_list_node_data (node);

      vcd_assert (n < 98);

      if (_sequence->id && !strcmp (item_id, _sequence->id))
	return n + 2;

      n++;
    }

  /* check entry points */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *_sequence = _vcd_list_node_data (node);
      VcdListNode *node2;

      /* default entry point */

      if (_sequence->default_entry_id 
	  && !strcmp (item_id, _sequence->default_entry_id))
	return n + 100;
      n++;

      /* additional entry points */

      _VCD_LIST_FOREACH (node2, _sequence->entry_list)
	{
	  entry_t *_entry = _vcd_list_node_data (node2);

	  vcd_assert (n < 500);

	  if (_entry->id && !strcmp (item_id, _entry->id))
	    return n + 100;

	  n++;
	}
    }

  /* check sequence items */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_segment_list)
    {
      mpeg_segment_t *_segment = _vcd_list_node_data (node);

      vcd_assert (n < 1980);

      if (_segment->id && !strcmp (item_id, _segment->id))
	return n + 1000;

      n += _segment->segment_count;
    }

  return 0;
}

bool
_vcd_pbc_available (const VcdObj *obj)
{
  vcd_assert (obj != NULL);
  vcd_assert (obj->pbc_list != NULL);

  if (!_vcd_list_length (obj->pbc_list))
    return false;

  if (!_vcd_obj_has_cap_p (obj, _CAP_PBC))
    {
      vcd_warn ("PBC list not empty but VCD type not capable of PBC!");
      return false;
    }

  return true;
}

uint16_t
_vcd_pbc_max_lid (const VcdObj *obj)
{
  uint16_t retval = 0;
  
  if (_vcd_pbc_available (obj))
    retval = _vcd_list_length (obj->pbc_list);

  return retval;
}

static size_t
_vcd_pbc_node_length (const VcdObj *obj, const pbc_t *_pbc, bool extended)
{
  size_t retval = 0;

  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  switch (_pbc->type)
    {
      int n;

    case PBC_PLAYLIST:
      n = _vcd_list_length (_pbc->item_id_list);
      retval = offsetof (PsdPlayListDescriptor, itemid[n]);
      break;

    case PBC_SELECTION:
      n = _vcd_list_length (_pbc->select_id_list);

      retval = offsetof (PsdSelectionListDescriptor, ofs[n]);

      if (extended || _vcd_obj_has_cap_p (obj, _CAP_4C_SVCD))
	retval += offsetof (PsdSelectionListDescriptorExtended, area[n]);
      break;
      
    case PBC_END:
      retval = sizeof (PsdEndListDescriptor);
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }

  return retval;
}

static uint16_t 
_lookup_psd_offset (const VcdObj *obj, const char item_id[], bool extended)
{
  VcdListNode *node;

  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  /* disable it */
  if (!item_id)
    return PSD_OFS_DISABLED;

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      if (!_pbc->id || strcmp (item_id, _pbc->id))
	continue;
	
      return (extended ? _pbc->offset_ext : _pbc->offset) / INFO_OFFSET_MULT;
    }

  vcd_error ("PSD: referenced PSD '%s' not found", item_id);
	    
  /* not found */
  return PSD_OFS_DISABLED;
}

static void
_vcd_pin_mark_id (const VcdObj *obj, const char _id[])
{
  mpeg_sequence_t *_seq;
  mpeg_segment_t *_seg;

  vcd_assert (obj != NULL);

  if (!_id)
    return;

  if ((_seq = _vcd_obj_get_sequence_by_id ((VcdObj *) obj, _id)))
    _seq->referenced = true;

  if ((_seg = _vcd_obj_get_segment_by_id ((VcdObj *) obj, _id)))
    _seg->referenced = true;
}

static void
_vcd_pbc_mark_id (const VcdObj *obj, const char _id[])
{
  pbc_t *_pbc;

  vcd_assert (obj != NULL);

  if (!_id)
    return;

  _pbc = _vcd_pbc_byid (obj, _id);

  if (!_pbc) /* not found */
    return;

  if (_pbc->referenced) /* already marked */
    return;

  _pbc->referenced = true;

  switch (_pbc->type)
    {
    case PBC_PLAYLIST:
      {
	VcdListNode *node;
	
	_vcd_pbc_mark_id (obj, _pbc->prev_id);
	_vcd_pbc_mark_id (obj, _pbc->next_id);
	_vcd_pbc_mark_id (obj, _pbc->retn_id);

	_VCD_LIST_FOREACH (node, _pbc->item_id_list)
	  {
	    const char *_id = _vcd_list_node_data (node);

	    _vcd_pin_mark_id (obj, _id);
	  }
      }
      break;

    case PBC_SELECTION:
      {
	VcdListNode *node;

	_vcd_pbc_mark_id (obj, _pbc->prev_id);
	_vcd_pbc_mark_id (obj, _pbc->next_id);
	_vcd_pbc_mark_id (obj, _pbc->retn_id);
	
	if (_pbc->selection_type == _SEL_NORMAL)
	  _vcd_pbc_mark_id (obj, _pbc->default_id);

	_vcd_pbc_mark_id (obj, _pbc->timeout_id);

	_vcd_pin_mark_id (obj, _pbc->item_id);

	_VCD_LIST_FOREACH (node, _pbc->select_id_list)
	  {
	    const char *_id = _vcd_list_node_data (node);

	    _vcd_pbc_mark_id (obj, _id);
	  }
      }      
      break;

    case PBC_END:
      _vcd_pin_mark_id (obj, _pbc->image_id);
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }
}

void
_vcd_pbc_check_unreferenced (const VcdObj *obj)
{
  VcdListNode *node;

  /* clear all flags */

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      _pbc->referenced = false;
    }

  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *_sequence = _vcd_list_node_data (node);

      _sequence->referenced = false;
    }

  _VCD_LIST_FOREACH (node, obj->mpeg_segment_list)
    {
      mpeg_segment_t *_segment = _vcd_list_node_data (node);

      _segment->referenced = false;
    }

  /* start from non-rejected lists */

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      vcd_assert (_pbc->id != NULL);

      if (_pbc->rejected)
	continue;

      _vcd_pbc_mark_id (obj, _pbc->id);
    }

  /* collect flags */

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      if (!_pbc->referenced)
	vcd_warn ("PSD item '%s' is unreachable", _pbc->id);
    }

  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *_sequence = _vcd_list_node_data (node);

      if (!_sequence->referenced)
	vcd_warn ("sequence '%s' is not reachable by PBC", _sequence->id);
    }

  _VCD_LIST_FOREACH (node, obj->mpeg_segment_list)
    {
      mpeg_segment_t *_segment = _vcd_list_node_data (node);

      if (!_segment->referenced)
	vcd_warn ("segment item '%s' is unreachable", _segment->id);
    }

}

void
_vcd_pbc_node_write (const VcdObj *obj, const pbc_t *_pbc, void *buf,
		     bool extended)
{
  vcd_assert (obj != NULL);
  vcd_assert (_pbc != NULL);
  vcd_assert (buf != NULL);

  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  switch (_pbc->type)
    {
    case PBC_PLAYLIST:
      {
	PsdPlayListDescriptor *_md = buf;
	VcdListNode *node;
	int n;
	
	_md->type = PSD_TYPE_PLAY_LIST;
	_md->noi = _vcd_list_length (_pbc->item_id_list);
	
	vcd_assert (_pbc->lid < 0x8000);
	_md->lid = uint16_to_be (_pbc->lid | (_pbc->rejected ? 0x8000 : 0));
	
	_md->prev_ofs = 
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->prev_id, extended));
	_md->next_ofs = 
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->next_id, extended));
	_md->return_ofs =
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->retn_id, extended));
	_md->ptime = uint16_to_be (rint (_pbc->playing_time * 15.0));
	_md->wtime = _wtime (_pbc->wait_time);
	_md->atime = _wtime (_pbc->auto_pause_time);
	
	n = 0;
	_VCD_LIST_FOREACH (node, _pbc->item_id_list)
	  {
	    const char *_id = _vcd_list_node_data (node);
	    uint16_t _pin;

	    if (_id)
	      {
		_pin = _vcd_pbc_pin_lookup (obj, _id);

		if (!_pin)
		  vcd_error ("PSD: referenced play item '%s' not found", _id);
		
		_md->itemid[n] = uint16_to_be (_pin); 
	      }
	    else
	      _md->itemid[n] = 0; /* play nothing */
	      
	    n++;
	  }
      }
      break;

    case PBC_SELECTION:
      {
	PsdSelectionListDescriptor *_md = buf;

	const unsigned _nos = _vcd_list_length (_pbc->select_id_list);

	if (extended)
	  _md->type = PSD_TYPE_EXT_SELECTION_LIST;
	else
	  _md->type = PSD_TYPE_SELECTION_LIST;

	if (!IN (_pbc->bsn, 1, 99))
	  vcd_error ("selection '%s': BSN (%d) not in range [1..99]",
		     _pbc->id, _pbc->bsn);

	if (!IN (_nos, 0, 99))
	  vcd_error ("selection '%s': too many selections (%d > 99)",
		     _pbc->id, _nos);

	if (_nos + _pbc->bsn > 100)
	  vcd_error ("selection '%s': BSN + NOS (%d + %d) > 100",
		     _pbc->id, _pbc->bsn, _nos);

	_md->bsn = _pbc->bsn;
	_md->nos = _nos;

	vcd_assert (sizeof (PsdSelectionListFlags) == 1);

	/* selection flags */
	if (_vcd_obj_has_cap_p (obj, _CAP_4C_SVCD))
	  _md->flags.SelectionAreaFlag = true;
	else
	  _md->flags.SelectionAreaFlag = false;

	_md->flags.CommandListFlag = false;

	vcd_assert (_pbc->lid < 0x8000);
	_md->lid = UINT16_TO_BE (_pbc->lid | (_pbc->rejected ? 0x8000 : 0));

	_md->prev_ofs = 
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->prev_id, extended));
	_md->next_ofs = 
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->next_id, extended));
	_md->return_ofs =
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->retn_id, extended));

	switch (_pbc->selection_type)
	  {
	  case _SEL_NORMAL:
	    _md->default_ofs =
	      uint16_to_be (_lookup_psd_offset (obj, _pbc->default_id, extended));
	    break;
	    
	  case _SEL_MULTI_DEF:
	    _md->default_ofs = uint16_to_be (PSD_OFS_MULTI_DEF);
	    if (_pbc->default_id)
	      vcd_warn ("ignoring default target '%s' for multi default selection '%s'",
			_pbc->default_id, _pbc->id);
	    break;

	  case _SEL_MULTI_DEF_NO_NUM:
	    _md->default_ofs = uint16_to_be (PSD_OFS_MULTI_DEF_NO_NUM);
	    if (_pbc->default_id)
	      vcd_warn ("ignoring default target '%s' for multi default (w/o num) selection '%s'",
			_pbc->default_id, _pbc->id);
	    break;
	    
	  default:
	    vcd_assert_not_reached ();
	    break;
	  }

	_md->timeout_ofs =
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->timeout_id, extended));
	_md->totime = _wtime (_pbc->timeout_time);

	if (_pbc->loop_count > 0x7f)
	  vcd_warn ("loop count %d > 127", _pbc->loop_count);

	_md->loop = (_pbc->loop_count > 0x7f) ? 0x7f : _pbc->loop_count;
	
	if (_pbc->jump_delayed)
	  _md->loop |= 0x80;

	/* timeout related sanity checks */
	if (_pbc->loop_count > 0
	    && _pbc->timeout_time >= 0
	    && !_pbc->timeout_id
	    && !_nos)
	  vcd_warn ("PSD: selection '%s': neither timeout nor select target available, but neither loop count is infinite nor timeout wait time", _pbc->id);

	if (_pbc->timeout_id && (_pbc->timeout_time < 0 || _pbc->loop_count <= 0))
	  vcd_warn ("PSD: selection '%s': timeout target '%s' is never used due to loop count or timeout wait time given", _pbc->id, _pbc->timeout_id);

	if (_pbc->item_id)
	  {
	    const uint16_t _pin = _vcd_pbc_pin_lookup (obj, _pbc->item_id);
	    
	    if (!_pin)
	      vcd_error ("PSD: referenced play item '%s' not found", _pbc->item_id);

	    _md->itemid = uint16_to_be (_pin);
	  }
	else
	  _md->itemid = 0; /* play nothing */

	/* sanity checks */
	switch (_pbc->selection_type)
	  {
	  case _SEL_NORMAL:
	    break;
	    
	  case _SEL_MULTI_DEF:
	  case _SEL_MULTI_DEF_NO_NUM:
	    if (_pbc->jump_delayed)
	      vcd_warn ("selection '%s': jump timing shall be immediate", _pbc->id);

	    if (_pbc->bsn != 1)
	      vcd_error ("selection '%s': BSN != 1 for multi default selection",
			 _pbc->id);

	    /* checking NOS == NOE */
	    if (!_pbc->item_id)
	      vcd_error ("selection '%s': play nothing play item not allowed for multidefault list",
			 _pbc->id);

	    {
	      mpeg_sequence_t *_seq;

	      if ((_seq = _vcd_obj_get_sequence_by_id ((VcdObj *) obj, _pbc->item_id))
		  || (_seq = _vcd_obj_get_sequence_by_entry_id ((VcdObj *) obj, _pbc->item_id)))
		{
		  const unsigned _entries = _vcd_list_length (_seq->entry_list) + 1;

		  if (_nos != _entries)
		    vcd_error ("selection '%s': number of entrypoints"
			       " (%d for sequence '%s') != number of selections (%d)",
			       _pbc->id, _entries, _pbc->item_id, _nos);
		}
	      else
		vcd_error ("selection '%s': play item '%s'"
			   " is requried to be sequence or entry point"
			   " item for multi default selecton",
			   _pbc->id, _pbc->item_id);
	    }	    

	    break;
	    
	  default:
	    vcd_assert_not_reached ();
	    break;
	  }

	/* fill selection array */
	{
	  VcdListNode *node = NULL;
	  int idx = 0;

	  idx = 0;
	  _VCD_LIST_FOREACH (node, _pbc->select_id_list)
	    {
	      const char *_id = _vcd_list_node_data (node);
	    
	      _md->ofs[idx] = 
		uint16_to_be (_lookup_psd_offset (obj, _id, extended));
	    
	      idx++;
	    }
	}

	if (extended || _vcd_obj_has_cap_p (obj, _CAP_4C_SVCD))
	  {
	    PsdSelectionListDescriptorExtended *_md2;
	    VcdListNode *node;
	    int n;
	    
	    /* append extended selection areas */

	    _md2 = (void *) &_md->ofs[_nos];

	    _set_area_helper (&_md2->next_area, _pbc->next_area, _pbc->id);
	    _set_area_helper (&_md2->prev_area, _pbc->prev_area, _pbc->id);
	    _set_area_helper (&_md2->return_area, _pbc->return_area, _pbc->id);

	    _set_area_helper (&_md2->default_area, _pbc->default_area, _pbc->id);

	    n = 0;
	    if (_pbc->select_area_list)
	      _VCD_LIST_FOREACH (node, _pbc->select_area_list)
	      {
		const pbc_area_t *_area = _vcd_list_node_data (node);

		_set_area_helper (&_md2->area[n], _area, _pbc->id);

		n++;
	      }

	    vcd_assert (n == _nos);
	  }
      }
      break;
      
    case PBC_END:
      {
	PsdEndListDescriptor *_md = buf;

	_md->type = PSD_TYPE_END_LIST;

	if (_vcd_obj_has_cap_p (obj, _CAP_4C_SVCD))
	  {
	    _md->next_disc = _pbc->next_disc;

	    if (_pbc->image_id)
	      {
		uint16_t _pin = _vcd_pbc_pin_lookup (obj, _pbc->image_id);
		mpeg_segment_t *_segment;

		if (!_pbc->next_disc)
		  vcd_warn ("PSD: endlist '%s': change disc picture given,"
			    " but next volume is 0", _pbc->id);

		if (!_pin)
		  vcd_error ("PSD: referenced play item '%s' not found", 
			     _pbc->item_id);

		_md->change_pic = UINT16_TO_BE (_pin);

		/* sanity checks */

		_segment = _vcd_obj_get_segment_by_id ((VcdObj *) obj,
						       _pbc->image_id);

		if (!_segment)
		  vcd_warn ("PSD: endlist '%s': referenced play item '%s'"
			    " is not a segment play item", 
			    _pbc->id, _pbc->image_id);
		else if (_segment->info->shdr[0].seen
			 || !(_segment->info->shdr[1].seen || _segment->info->shdr[2].seen))
		  vcd_warn ("PSD: endlist '%s': referenced play item '%s'"
			    " should be a still picture", 
			    _pbc->id, _pbc->image_id);
	      }
	  }
	else if (_pbc->next_disc || _pbc->image_id)
	  vcd_warn ("extended end list attributes ignored for non-SVCD");
      }
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }
}

pbc_t *
vcd_pbc_new (enum pbc_type_t type)
{
  pbc_t *_pbc;

  _pbc = _vcd_malloc (sizeof (pbc_t));
  _pbc->type = type;

  switch (type)
    {
    case PBC_PLAYLIST:
      _pbc->item_id_list = _vcd_list_new ();
      break;

    case PBC_SELECTION:
      _pbc->select_id_list = _vcd_list_new ();
      _pbc->select_area_list = _vcd_list_new ();
      break;
      
    case PBC_END:
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }

  return _pbc;
}

/* 
 */

bool
_vcd_pbc_finalize (VcdObj *obj)
{
  VcdListNode *node;
  unsigned offset = 0, offset_ext = 0;
  unsigned lid;

  lid = 1;
  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);
      unsigned length, length_ext = 0;

      length = _vcd_pbc_node_length (obj, _pbc, false);
      if (_vcd_obj_has_cap_p (obj, _CAP_PBC_X))
	length_ext = _vcd_pbc_node_length (obj, _pbc, true);

      /* round them up to... */
      length = _vcd_ceil2block (length, INFO_OFFSET_MULT);
      if (_vcd_obj_has_cap_p (obj, _CAP_PBC_X))
	length_ext = _vcd_ceil2block (length_ext, INFO_OFFSET_MULT);

      /* node may not cross sector boundary! */
      offset = _vcd_ofs_add (offset, length, ISO_BLOCKSIZE);
      if (_vcd_obj_has_cap_p (obj, _CAP_PBC_X))
	offset_ext = _vcd_ofs_add (offset_ext, length_ext, ISO_BLOCKSIZE);

      /* save start offsets */
      _pbc->offset = offset - length;
      if (_vcd_obj_has_cap_p (obj, _CAP_PBC_X))
	_pbc->offset_ext = offset_ext - length_ext;

      _pbc->lid = lid;

      lid++;
    }

  obj->psd_size = offset;
  if (_vcd_obj_has_cap_p (obj, _CAP_PBC_X))
    obj->psdx_size = offset_ext;

  vcd_debug ("pbc: psd size %d (extended psd %d)", offset, offset_ext);

  return true;
}
