/*
 * $Id: wpa.c 10052 2010-12-01 16:43:51Z dmytrof $
 *
 * Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2009 Metalink Broadband (Israel)
 *
 * WPA security helper routines
 *
 */

#include "mtlkinc.h"
#include "umi_rsn.h"
#include "wpa.h"

#define LOG_LOCAL_GID   GID_WPA
#define LOG_LOCAL_FID   1

static const int wpa_selector_len = 4;
static const u8 wpa_oui_type[] = { 0x00, 0x50, 0xf2, 1 };
static const u16 wpa_version = 1;
static const u8 wpa_auth_key_mgmt_none[] = { 0x00, 0x50, 0xf2, 0 };
static const u8 wpa_auth_key_mgmt_unspec_802_1x[] = { 0x00, 0x50, 0xf2, 1 };
static const u8 wpa_auth_key_mgmt_psk_over_802_1x[] = { 0x00, 0x50, 0xf2, 2 };
static const u8 umi_rsn_cipher_suite_suite_none[] = { 0x00, 0x50, 0xf2, 0 };
static const u8 umi_rsn_cipher_suite_suite_wep40[] = { 0x00, 0x50, 0xf2, 1 };
static const u8 umi_rsn_cipher_suite_suite_tkip[] = { 0x00, 0x50, 0xf2, 2 };
static const u8 umi_rsn_cipher_suite_suite_wrap[] = { 0x00, 0x50, 0xf2, 3 };
static const u8 umi_rsn_cipher_suite_suite_ccmp[] = { 0x00, 0x50, 0xf2, 4 };
static const u8 umi_rsn_cipher_suite_suite_wep104[] = { 0x00, 0x50, 0xf2, 5 };

/* WPA IE version 1
 * 00-50-f2:1 (OUI:OUI type)
 * 0x01 0x00 (version; little endian)
 * (all following fields are optional:)
 * Group Suite Selector (4 octets) (default: TKIP)
 * Pairwise Suite Count (2 octets, little endian) (default: 1)
 * Pairwise Suite List (4 * n octets) (default: TKIP)
 * Authenticated Key Management Suite Count (2 octets, little endian)
 *    (default: 1)
 * Authenticated Key Management Suite List (4 * n octets)
 *    (default: unspec 802.1X)
 * WPA Capabilities (2 octets, little endian) (default: 0)
 */

struct wpa_ie_hdr {
        u8 elem_id;
        u8 len;
        u8 oui[3];
        u8 oui_type;
        u8 version[2];
} __attribute__ ((packed));


static const int rsn_selector_len = 4;
static const u16 rsn_version = 1;
static const u8 rsn_auth_key_mgmt_unspec_802_1x[] = { 0x00, 0x0f, 0xac, 1 };
static const u8 rsn_auth_key_mgmt_psk_over_802_1x[] = { 0x00, 0x0f, 0xac, 2 };
static const u8 rsn_cipher_suite_none[] = { 0x00, 0x0f, 0xac, 0 };
static const u8 rsn_cipher_suite_wep40[] = { 0x00, 0x0f, 0xac, 1 };
static const u8 rsn_cipher_suite_tkip[] = { 0x00, 0x0f, 0xac, 2 };
static const u8 rsn_cipher_suite_wrap[] = { 0x00, 0x0f, 0xac, 3 };
static const u8 rsn_cipher_suite_ccmp[] = { 0x00, 0x0f, 0xac, 4 };
static const u8 rsn_cipher_suite_wep104[] = { 0x00, 0x0f, 0xac, 5 };

/* EAPOL-Key Key Data Encapsulation
 * GroupKey and STAKey require encryption, otherwise, encryption is optional.
 */
static const u8 rsn_key_data_groupkey[] = { 0x00, 0x0f, 0xac, 1 };
static const u8 rsn_key_data_stakey[] = { 0x00, 0x0f, 0xac, 2 };
static const u8 rsn_key_data_mac_addr[] = { 0x00, 0x0f, 0xac, 3 };
static const u8 rsn_key_data_pmkid[] = { 0x00, 0x0f, 0xac, 4 };

/* 1/4: PMKID
 * 2/4: RSN IE
 * 3/4: one or two RSN IEs + GTK IE (encrypted)
 * 4/4: empty
 * 1/2: GTK IE (encrypted)
 * 2/2: empty
 */

/* RSN IE version 1
 * 0x01 0x00 (version; little endian)
 * (all following fields are optional:)
 * Group Suite Selector (4 octets) (default: CCMP)
 * Pairwise Suite Count (2 octets, little endian) (default: 1)
 * Pairwise Suite List (4 * n octets) (default: CCMP)
 * Authenticated Key Management Suite Count (2 octets, little endian)
 *    (default: 1)
 * Authenticated Key Management Suite List (4 * n octets)
 *    (default: unspec 802.1X)
 * RSN Capabilities (2 octets, little endian) (default: 0)
 * PMKID Count (2 octets) (default: 0)
 * PMKID List (16 * n octets)
 */

struct rsn_ie_hdr {
        u8 elem_id; /* WLAN_EID_RSN */
        u8 len;
        u8 version[2];
} __attribute__ ((packed));


static int wpa_selector_to_bitfield(const u8 *s)
{
  if (memcmp(s, umi_rsn_cipher_suite_suite_none, wpa_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_NONE;
  if (memcmp(s, umi_rsn_cipher_suite_suite_wep40, wpa_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_WEP40;
  if (memcmp(s, umi_rsn_cipher_suite_suite_tkip, wpa_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_TKIP;
  if (memcmp(s, umi_rsn_cipher_suite_suite_ccmp, wpa_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_CCMP;
  if (memcmp(s, umi_rsn_cipher_suite_suite_wep104, wpa_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_WEP104;
  return 0;
}


static int wpa_key_mgmt_to_bitfield(const u8 *s)
{
  if (memcmp(s, wpa_auth_key_mgmt_unspec_802_1x, wpa_selector_len) == 0)
    return WPA_KEY_MGMT_IEEE8021X;
  if (memcmp(s, wpa_auth_key_mgmt_psk_over_802_1x, wpa_selector_len) ==
      0)
    return WPA_KEY_MGMT_PSK;
  if (memcmp(s, wpa_auth_key_mgmt_none, wpa_selector_len) == 0)
    return WPA_KEY_MGMT_WPA_NONE;
  return 0;
}


static int rsn_selector_to_bitfield(const u8 *s)
{
  if (memcmp(s, rsn_cipher_suite_none, rsn_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_NONE;
  if (memcmp(s, rsn_cipher_suite_wep40, rsn_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_WEP40;
  if (memcmp(s, rsn_cipher_suite_tkip, rsn_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_TKIP;
  if (memcmp(s, rsn_cipher_suite_ccmp, rsn_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_CCMP;
  if (memcmp(s, rsn_cipher_suite_wep104, rsn_selector_len) == 0)
    return UMI_RSN_CIPHER_SUITE_WEP104;
  return 0;
}


static int rsn_key_mgmt_to_bitfield(const u8 *s)
{
  if (memcmp(s, rsn_auth_key_mgmt_unspec_802_1x, rsn_selector_len) == 0)
    return WPA_KEY_MGMT_IEEE8021X;
  if (memcmp(s, rsn_auth_key_mgmt_psk_over_802_1x, rsn_selector_len) ==
      0)
    return WPA_KEY_MGMT_PSK;
  return 0;
}


#define WPA_GET_LE16(a) ((u16) (((a)[1] << 8) | (a)[0]))

static int wpa_parse_wpa_ie_wpa(const u8 *wpa_ie, size_t wpa_ie_len,
        struct wpa_ie_data *data)
{
  const struct wpa_ie_hdr *hdr;
  const u8 *pos;
  int left;
  int i, count;

  data->proto = WPA_PROTO_WPA;
  data->pairwise_cipher = UMI_RSN_CIPHER_SUITE_TKIP;
  data->group_cipher = UMI_RSN_CIPHER_SUITE_TKIP;
  data->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
  data->capabilities = 0;
  data->pmkid = NULL;
  data->num_pmkid = 0;

  if (wpa_ie_len == 0) {
    /* No WPA IE - fail silently */
    return -1;
  }

  if (wpa_ie_len < sizeof(struct wpa_ie_hdr)) {
    ELOG_D("ie len too short %lu",
         (unsigned long) wpa_ie_len);
    return -1;
  }

  hdr = (const struct wpa_ie_hdr *) wpa_ie;

  if (hdr->elem_id != GENERIC_INFO_ELEM ||
      hdr->len > wpa_ie_len - 2 ||
      memcmp(&hdr->oui, wpa_oui_type, wpa_selector_len) != 0 ||
      WPA_GET_LE16(hdr->version) != wpa_version) {
    ELOG_V("malformed ie or unknown version");
    return -1;
  }

  pos = (const u8 *) (hdr + 1);
  left = wpa_ie_len - sizeof(*hdr);

  if (left >= wpa_selector_len) {
    data->group_cipher = wpa_selector_to_bitfield(pos);
    pos += wpa_selector_len;
    left -= wpa_selector_len;
  } else if (left > 0) {
    ELOG_D("ie length mismatch, %u too much", left);
    return -1;
  }

  if (left >= 2) {
    data->pairwise_cipher = 0;
    count = WPA_GET_LE16(pos);
    pos += 2;
    left -= 2;
    if (count == 0 || left < count * wpa_selector_len) {
      ELOG_DD("ie count botch (pairwise), count %u left %u", count, left);
      return -1;
    }
    for (i = 0; i < count; i++) {
      data->pairwise_cipher |= wpa_selector_to_bitfield(pos);
      pos += wpa_selector_len;
      left -= wpa_selector_len;
    }
  } else if (left == 1) {
    ELOG_V("ie too short (for key mgmt)");
    return -1;
  }

  if (left >= 2) {
    data->key_mgmt = 0;
    count = WPA_GET_LE16(pos);
    pos += 2;
    left -= 2;
    if (count == 0 || left < count * wpa_selector_len) {
      ELOG_DD("ie count botch (key mgmt), count %u left %u", count, left);
      return -1;
    }
    for (i = 0; i < count; i++) {
      data->key_mgmt |= wpa_key_mgmt_to_bitfield(pos);
      pos += wpa_selector_len;
      left -= wpa_selector_len;
    }
  } else if (left == 1) {
    ELOG_V("ie too short (for capabilities)");
    return -1;
  }

  if (left >= 2) {
    data->capabilities = WPA_GET_LE16(pos);
    pos += 2;
    left -= 2;
  }

  if (left > 0)
    WLOG_D("ie has %u trailing bytes - ignored", left);

  return 0;
}


static int wpa_parse_wpa_ie_rsn(const u8 *rsn_ie, size_t rsn_ie_len,
        struct wpa_ie_data *data)
{
  const struct rsn_ie_hdr *hdr;
  const u8 *pos;
  int left;
  int i, count;

  data->proto = WPA_PROTO_RSN;
  data->pairwise_cipher = UMI_RSN_CIPHER_SUITE_CCMP;
  data->group_cipher = UMI_RSN_CIPHER_SUITE_CCMP;
  data->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
  data->capabilities = 0;
  data->pmkid = NULL;
  data->num_pmkid = 0;

  if (rsn_ie_len == 0) {
    /* No RSN IE - fail silently */
    return -1;
  }

  if (rsn_ie_len < sizeof(struct rsn_ie_hdr)) {
    ELOG_D("ie len too short %lu",
         (unsigned long) rsn_ie_len);
    return -1;
  }

  hdr = (const struct rsn_ie_hdr *) rsn_ie;

  if (hdr->elem_id != RSN_INFO_ELEM ||
      hdr->len > rsn_ie_len - 2 ||
      WPA_GET_LE16(hdr->version) != rsn_version) {
    ELOG_V("malformed ie or unknown version");
    return -1;
  }

  pos = (const u8 *) (hdr + 1);
  left = rsn_ie_len - sizeof(*hdr);

  if (left >= rsn_selector_len) {
    data->group_cipher = rsn_selector_to_bitfield(pos);
    pos += rsn_selector_len;
    left -= rsn_selector_len;
  } else if (left > 0) {
    ELOG_D("ie length mismatch, %u too much",
         left);
    return -1;
  }

  if (left >= 2) {
    data->pairwise_cipher = 0;
    count = WPA_GET_LE16(pos);
    pos += 2;
    left -= 2;
    if (count == 0 || left < count * rsn_selector_len) {
      ELOG_DD("ie count botch (pairwise), count %u left %u", count, left);
      return -1;
    }
    for (i = 0; i < count; i++) {
      data->pairwise_cipher |= rsn_selector_to_bitfield(pos);
      pos += rsn_selector_len;
      left -= rsn_selector_len;
    }
  } else if (left == 1) {
    ELOG_V("ie too short (for key mgmt)");
    return -1;
  }

  if (left >= 2) {
    data->key_mgmt = 0;
    count = WPA_GET_LE16(pos);
    pos += 2;
    left -= 2;
    if (count == 0 || left < count * rsn_selector_len) {
      ELOG_DD("ie count botch (key mgmt), count %u left %u", count, left);
      return -1;
    }
    for (i = 0; i < count; i++) {
      data->key_mgmt |= rsn_key_mgmt_to_bitfield(pos);
      pos += rsn_selector_len;
      left -= rsn_selector_len;
    }
  } else if (left == 1) {
    ELOG_V("ie too short (for capabilities)");
    return -1;
  }

  if (left >= 2) {
    data->capabilities = WPA_GET_LE16(pos);
    pos += 2;
    left -= 2;
  }

  if (left >= 2) {
    data->num_pmkid = WPA_GET_LE16(pos);
    pos += 2;
    left -= 2;
    if (left < data->num_pmkid * PMKID_LEN) {
      ELOG_DD("PMKID underflow (num_pmkid=%d left=%d)",
           data->num_pmkid, left);
      data->num_pmkid = 0;
    } else {
      data->pmkid = pos;
      pos += data->num_pmkid * PMKID_LEN;
      left -= data->num_pmkid * PMKID_LEN;
    }
  }

  if (left > 0)
    WLOG_D("ie has %u trailing bytes - ignored", left);

  return 0;
}


/**
 * wpa_parse_wpa_ie - Parse WPA/RSN IE
 * @wpa_ie: Pointer to WPA or RSN IE
 * @wpa_ie_len: Length of the WPA/RSN IE
 * @data: Pointer to data area for parsing results
 * Returns: 0 on success, -1 on failure
 *
 * Parse the contents of WPA or RSN IE and write the parsed data into data.
 */
int wpa_parse_wpa_ie(const u8 *wpa_ie, size_t wpa_ie_len,
         struct wpa_ie_data *data)
{
  if (wpa_ie_len >= 1 && wpa_ie[0] == RSN_INFO_ELEM)
    return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
  else
    return wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, data);
}

