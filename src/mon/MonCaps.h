// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2009 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 * MonCaps: Hold the capabilities associated with a single authenticated 
 * user key. These are specified by text strings of the form
 * "allow r" (which allows reading of the cluster state)
 * "allow rwx auid foo[,bar,baz]" (which allows full access to listed auids)
 * "allow rw service_name" (which allows reading and writing to the named
 *                          service type)
 * "allow *" (which allows full access to EVERYTHING)
 *
 * When the monitor is checking permissions, besides the obvious read and write
 * it generally equates having an execute permission with being of the
 * associated type. So, in instances where it only wants to receive a
 * certain kind of message from OSDs, it will require a MON_CAP_X on
 * PAXOS_OSDMAP.
 */

#ifndef CEPH_MONCAPS_H
#define CEPH_MONCAPS_H

#include "include/types.h"

#define MON_CAP_R 0x1
#define MON_CAP_W 0x2
#define MON_CAP_X 0x4

#define MON_CAP_RW  (MON_CAP_R | MON_CAP_W)
#define MON_CAP_RX  (MON_CAP_R | MON_CAP_X)
#define MON_CAP_ALL (MON_CAP_R | MON_CAP_W | MON_CAP_X)

typedef __u8 rwx_t;

struct MonCap {
  rwx_t allow;
  rwx_t deny;

  MonCap() : allow(0), deny(0) {}
  MonCap(rwx_t a, rwx_t d) : allow(a), deny(d) {}

  void encode(bufferlist& bl) const {
    ::encode(allow, bl);
    ::encode(deny, bl);
  }

  void decode(bufferlist::iterator& bl) {
    ::decode(allow, bl);
    ::decode(deny, bl);
  }
  void dump(Formatter *f) const;
  static void generate_test_instances(list<MonCap*>& o);
};
WRITE_RAW_ENCODER(MonCap);

struct MonCaps {
  string text;
  rwx_t default_action;
  map<int, MonCap> services_map;
  map<int, MonCap> pool_auid_map;
  bool allow_all;
  uint64_t auid;

  // command whitelist
  list<list<string> > cmd_allow;

  bool get_next_token(string s, size_t& pos, string& token);
  bool is_rwx(CephContext *cct, string& token, rwx_t& cap_val);
  int get_service_id(string& token);

public:
  MonCaps()
    : text(), default_action(0),
      allow_all(false), auid(CEPH_AUTH_UID_DEFAULT)
  {}

  const string& get_str() const { return text; }

  bool parse(string s, CephContext *cct);
  bool parse(bufferlist::iterator& iter, CephContext *cct);
  rwx_t get_caps(int service) const;
  bool check_privileges(int service, int req_perm,
			uint64_t auid=CEPH_AUTH_UID_DEFAULT);
  void set_allow_all(bool allow) { allow_all = allow; }
  void set_auid(uint64_t uid) { auid = uid; }

  bool get_allow_all() const {
    return allow_all;
  }

  void encode(bufferlist& bl) const;
  void decode(bufferlist::iterator& bl);
  void dump(Formatter *f) const;
  static void generate_test_instances(list<MonCaps*>& o);
};
WRITE_CLASS_ENCODER(MonCaps);

inline ostream& operator<<(ostream& out, const MonCaps& m) {
  return out << m.get_str();
}

#endif
