// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab

#include "include/buffer.h"
#include "include/encoding.h"

#include "cls_rbd_client.h"

#include <errno.h>

namespace librbd {
  namespace cls_client {
    int get_immutable_metadata(librados::IoCtx *ioctx, const std::string &oid,
			       std::string *object_prefix, uint8_t *order)
    {
      assert(object_prefix);
      assert(order);

      librados::ObjectReadOperation op;
      bufferlist bl, empty;
      snapid_t snap = CEPH_NOSNAP;
      ::encode(snap, bl);
      op.exec("rbd", "get_size", bl);
      op.exec("rbd", "get_object_prefix", empty);

      bufferlist outbl;
      ioctx->operate(oid, &op, &outbl);

      try {
	bufferlist::iterator iter = outbl.begin();
	uint64_t size;
	::decode(*order, iter);
	::decode(size, iter);
	::decode(*object_prefix, iter);
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      return 0;
    }

    int get_mutable_metadata(librados::IoCtx *ioctx, const std::string &oid,
			     uint64_t *size, uint64_t *features,
			     uint64_t *incompatible_features,
			     ::SnapContext *snapc)
    {
      assert(size);
      assert(features);
      assert(incompatible_features);
      assert(snapc);

      librados::ObjectReadOperation op;
      bufferlist sizebl, featuresbl, empty;
      snapid_t snap = CEPH_NOSNAP;
      ::encode(snap, sizebl);
      ::encode(snap, featuresbl);
      op.exec("rbd", "get_size", sizebl);
      op.exec("rbd", "get_features", featuresbl);
      op.exec("rbd", "get_snapcontext", empty);

      bufferlist outbl;
      ioctx->operate(oid, &op, &outbl);

      try {
	bufferlist::iterator iter = outbl.begin();
	uint8_t order;
	::decode(order, iter);
	::decode(*size, iter);
	::decode(*features, iter);
	::decode(*incompatible_features, iter);
	::decode(*snapc, iter);
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      return 0;
    }

    int create_image(librados::IoCtx *ioctx, const std::string &oid,
		     uint64_t size, uint8_t order, uint64_t features,
		     const std::string &object_prefix)
    {
      bufferlist bl, bl2;
      ::encode(size, bl);
      ::encode(order, bl);
      ::encode(features, bl);
      ::encode(object_prefix, (bl));

      return ioctx->exec(oid, "rbd", "create", bl, bl2);
    }

    int get_features(librados::IoCtx *ioctx, const std::string &oid,
		     uint64_t snap_id, uint64_t *features)
    {
      bufferlist inbl, outbl;
      ::encode(snap_id, inbl);

      int r = ioctx->exec(oid, "rbd", "get_features", inbl, outbl);
      if (r < 0)
	return r;

      try {
	bufferlist::iterator iter = outbl.begin();
	::decode(*features, iter);
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      return 0;
    }

    int get_object_prefix(librados::IoCtx *ioctx, const std::string &oid,
			  std::string *object_prefix)
    {
      bufferlist inbl, outbl;
      int r = ioctx->exec(oid, "rbd", "get_object_prefix", inbl, outbl);
      if (r < 0)
	return r;

      try {
	bufferlist::iterator iter = outbl.begin();
	::decode(*object_prefix, iter);
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      return 0;
    }

    int get_size(librados::IoCtx *ioctx, const std::string &oid,
		 uint64_t snap_id, uint64_t *size, uint8_t *order)
    {
      bufferlist inbl, outbl;
      ::encode(snap_id, inbl);

      int r = ioctx->exec(oid, "rbd", "get_size", inbl, outbl);
      if (r < 0)
	return r;

      try {
	bufferlist::iterator iter = outbl.begin();
	::decode(*order, iter);
	::decode(*size, iter);
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      return 0;
    }

    int set_size(librados::IoCtx *ioctx, const std::string &oid,
		 uint64_t size)
    {
      bufferlist bl, bl2;
      ::encode(size, bl);

      return ioctx->exec(oid, "rbd", "set_size", bl, bl2);
    }

    int snapshot_add(librados::IoCtx *ioctx, const std::string &oid,
		     uint64_t snap_id, const std::string &snap_name)
    {
      bufferlist bl, bl2;
      ::encode(snap_name, bl);
      ::encode(snap_id, bl);

      return ioctx->exec(oid, "rbd", "snapshot_add", bl, bl2);
    }

    int snapshot_remove(librados::IoCtx *ioctx, const std::string &oid,
			uint64_t snap_id)
    {
      bufferlist bl, bl2;
      ::encode(snap_id, bl);

      return ioctx->exec(oid, "rbd", "snapshot_remove", bl, bl2);
    }

    int get_snapcontext(librados::IoCtx *ioctx, const std::string &oid,
			::SnapContext *snapc)
    {
      bufferlist inbl, outbl;

      int r = ioctx->exec(oid, "rbd", "get_snapcontext", inbl, outbl);
      if (r < 0)
	return r;

      try {
	bufferlist::iterator iter = outbl.begin();
	::decode(*snapc, iter);
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      if (!snapc->is_valid())
	return -EBADMSG;

      return 0;
    }

    int snapshot_list(librados::IoCtx *ioctx, const std::string &oid,
		      const std::vector<snapid_t> &ids,
		      std::vector<string> *names,
		      std::vector<uint64_t> *sizes,
		      std::vector<uint64_t> *features)
    {
      names->clear();
      names->resize(ids.size());
      sizes->clear();
      sizes->resize(ids.size());
      features->clear();
      features->resize(ids.size());
      librados::ObjectReadOperation op;
      for (vector<snapid_t>::const_iterator it = ids.begin();
	   it != ids.end(); ++it) {
	bufferlist bl1, bl2, bl3;
	uint64_t snap_id = it->val;
	::encode(snap_id, bl1);
	op.exec("rbd", "get_snapshot_name", bl1);
	::encode(snap_id, bl2);
	op.exec("rbd", "get_size", bl2);
	::encode(snap_id, bl3);
	op.exec("rbd", "get_features", bl3);
      }

      bufferlist outbl;
      int r = ioctx->operate(oid, &op, &outbl);
      if (r < 0)
	return r;

      try {
	bufferlist::iterator iter = outbl.begin();
	for (size_t i = 0; i < ids.size(); ++i) {
	  uint8_t order;
	  uint64_t incompat_features;
	  ::decode((*names)[i], iter);
	  ::decode(order, iter);
	  ::decode((*sizes)[i], iter);
	  ::decode((*features)[i], iter);
	  ::decode(incompat_features, iter);
	}
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      return 0;
    }

    int assign_bid(librados::IoCtx *ioctx, const std::string &oid,
		   uint64_t *id)
    {
      bufferlist bl, out;
      int r = ioctx->exec(oid, "rbd", "assign_bid", bl, out);
      if (r < 0)
	return r;

      try {
	bufferlist::iterator iter = out.begin();
	::decode(*id, iter);
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      return 0;
    }

    int old_snapshot_add(librados::IoCtx *ioctx, const std::string &oid,
			 uint64_t snap_id, const std::string &snap_name)
    {
      bufferlist bl, bl2;
      ::encode(snap_name, bl);
      ::encode(snap_id, bl);

      return ioctx->exec(oid, "rbd", "snap_add", bl, bl2);
    }

    int old_snapshot_remove(librados::IoCtx *ioctx, const std::string &oid,
			    const std::string &snap_name)
    {
      bufferlist bl, bl2;
      ::encode(snap_name, bl);

      return ioctx->exec(oid, "rbd", "snap_remove", bl, bl2);
    }

    int old_snapshot_list(librados::IoCtx *ioctx, const std::string &oid,
			  std::vector<string> *names,
			  std::vector<uint64_t> *sizes,
			  ::SnapContext *snapc)
    {
      bufferlist bl, outbl;
      int r = ioctx->exec(oid, "rbd", "snap_list", bl, outbl);
      if (r < 0)
	return r;

      bufferlist::iterator iter = outbl.begin();
      uint32_t num_snaps;
      try {
	::decode(snapc->seq, iter);
	::decode(num_snaps, iter);

	names->resize(num_snaps);
	sizes->resize(num_snaps);
	snapc->snaps.resize(num_snaps);

	for (uint32_t i = 0; i < num_snaps; ++i) {
	  ::decode(snapc->snaps[i], iter);
	  ::decode((*sizes)[i], iter);
	  ::decode((*names)[i], iter);
	}
      } catch (const buffer::error &err) {
	return -EBADMSG;
      }

      return 0;
    }
  } // namespace cls_client
} // namespace librbd
