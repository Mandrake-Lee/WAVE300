/*
 * hostapd / Driver interaction with Lantiq Wave300/400 driver
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 * Copyright (c) 2011      Lantiq (Israel)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <net/if_arp.h>

#include "common.h"
#include "driver.h"
#include "driver_wext.h"
#include "eloop.h"
#include "common/ieee802_11_defs.h"
#include "wireless_copy.h"

#include "priv_netlink.h"
#include "netlink.h"
#include "l2_packet/l2_packet.h"
#include "radius/radius.h"


enum ietypes {
	IE_WSC_BEACON     = 0,
	IE_WSC_PROBE_REQ  = 1,
	IE_WSC_PROBE_RESP = 2,
	IE_WSC_ASSOC_REQ  = 3,
	IE_WSC_ASSOC_RESP = 4
};

struct mtlk_driver_data {
	struct hostapd_data *hapd;		/* back pointer */

	char	iface[IFNAMSIZ + 1];
	int     ifindex;
	struct l2_packet_data *sock_xmit;	/* raw packet xmit socket */
	struct l2_packet_data *sock_recv;	/* raw packet recv socket */
	int	ioctl_sock;			/* socket for ioctl() use */
	struct netlink_data *netlink;
	int	we_version;
	u8	acct_mac[ETH_ALEN];
	struct hostap_sta_driver_data acct_data;
};

static const char *
ether_sprintf(const u8 *addr)
{
	static char buf[sizeof(MACSTR)];

	if (addr != NULL)
		snprintf(buf, sizeof(buf), MACSTR, MAC2STR(addr));
	else
		snprintf(buf, sizeof(buf), MACSTR, 0,0,0,0,0,0);
	return buf;
}

int
mtlk_set_iface_flags(void *priv, int dev_up)
{
	struct mtlk_driver_data *drv = priv;
	struct ifreq ifr;

	wpa_printf(MSG_DEBUG, "%s: dev_up=%d", __func__, dev_up);

	if (drv->ioctl_sock < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", drv->iface);

	if (ioctl(drv->ioctl_sock, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		return -1;
	}

	if (dev_up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(drv->ioctl_sock, SIOCSIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCSIFFLAGS]");
		return -1;
	}

	return 0;
}

static int
mtlk_set_encryption(const char *ifname, void *priv, enum wpa_alg alg,
	     const u8 *addr, int key_idx, int txkey,
	     const u8 *seq, size_t seq_len,
	     const u8 *key, size_t key_len)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	struct iw_encode_ext *ext;
	int ret=0;

	wpa_printf(MSG_DEBUG,
		"%s: alg=%d addr=%s key_idx=%d txkey=%d",
		__func__, alg, ether_sprintf(addr), key_idx, txkey);

	ext = malloc(sizeof(*ext) + key_len);
	if (ext == NULL)
		return -1;
	memset(ext, 0, sizeof(*ext) + key_len);
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.encoding.flags = key_idx + 1;
	iwr.u.encoding.pointer = (caddr_t) ext;
	iwr.u.encoding.length = sizeof(*ext) + key_len;

	if (addr == NULL ||
	    memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0)
		ext->ext_flags |= IW_ENCODE_EXT_GROUP_KEY;
	if (txkey)
		ext->ext_flags |= IW_ENCODE_EXT_SET_TX_KEY;
	
	ext->addr.sa_family = ARPHRD_ETHER;
	if (addr)
		memcpy(ext->addr.sa_data, addr, ETH_ALEN);
	else
		memset(ext->addr.sa_data, 0xff, ETH_ALEN);
	if (key && key_len) {
		memcpy(ext + 1, key, key_len);
		ext->key_len = key_len;
	}

	if (alg == WPA_ALG_NONE)
		ext->alg = IW_ENCODE_ALG_NONE;
	else if (alg == WPA_ALG_WEP)
		ext->alg = IW_ENCODE_ALG_WEP;
	else if (alg == WPA_ALG_TKIP)
		ext->alg = IW_ENCODE_ALG_TKIP;
	else if (alg == WPA_ALG_CCMP)
		ext->alg = IW_ENCODE_ALG_CCMP;
	else {
		printf("%s: unknown/unsupported algorithm %d\n",
			__func__, alg);
		return -1;
	}

	if (ioctl(drv->ioctl_sock, SIOCSIWENCODEEXT, &iwr) < 0) {
		ret = errno == EOPNOTSUPP ? -2 : -1;
		perror("ioctl[SIOCSIWENCODEEXT]");
	}

	free(ext);
	return ret;
}


static int
mtlk_get_seqnum(const char *ifname, void *priv, const u8 *addr, int idx, u8 *seq)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	struct iw_encode_ext *ext;
	int ret=0;

	wpa_printf(MSG_DEBUG,
		"%s: addr=%s idx=%d", __func__, ether_sprintf(addr), idx);

	ext = malloc(sizeof(*ext));
	if (ext == NULL)
		return -1;
	memset(ext, 0, sizeof(*ext));
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.encoding.pointer = (caddr_t) ext;
	iwr.u.encoding.length = sizeof(*ext);

	if (addr == NULL ||
	    memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0)
		iwr.u.encoding.flags |= IW_ENCODE_EXT_GROUP_KEY;

	if (ioctl(drv->ioctl_sock, SIOCGIWENCODEEXT, &iwr) < 0) {
		ret = errno == EOPNOTSUPP ? -2 : -1;
		perror("ioctl[SIOCGIWENCODEEXT]");
		goto err;
	}

	memcpy(seq, ext->rx_seq, 6);
err:
	free(ext);
	return ret;
}


static int 
mtlk_flush(void *priv)
{
	return 0;		/* XXX */
}



static int
mtlk_sta_clear_stats(void *priv, const u8 *addr)
{
	return 0; /* FIX */
}


static int
mtlk_set_generic_elem(void *priv, const u8 *ie, size_t ie_len)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) ie;
	iwr.u.data.length = ie_len;

	if (ioctl(drv->ioctl_sock, SIOCSIWGENIE, &iwr) < 0) {
		perror("ioctl[SIOCSIWGENIE]");
		ret = -1;
	}

	return ret;
}

static int
mtlk_mlme(struct mtlk_driver_data *drv,
	const u8 *addr, int cmd, int reason_code)
{
	struct iwreq iwr;
	struct iw_mlme mlme;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	memset(&mlme, 0, sizeof(mlme));
	mlme.cmd = cmd;
	mlme.reason_code = reason_code;
	mlme.addr.sa_family = ARPHRD_ETHER;
	memcpy(mlme.addr.sa_data, addr, ETH_ALEN);
	iwr.u.data.pointer = (caddr_t) &mlme;
	iwr.u.data.length = sizeof(mlme);

	if (ioctl(drv->ioctl_sock, SIOCSIWMLME, &iwr) < 0) {
		perror("ioctl[SIOCSIWMLME]");
		ret = -1;
	}

	return ret;
}

static int
mtlk_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr, int reason_code)
{
	struct mtlk_driver_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return mtlk_mlme(drv, addr, IW_MLME_DEAUTH, reason_code);
}

static int
mtlk_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr, int reason_code)
{
	struct mtlk_driver_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return mtlk_mlme(drv, addr, IW_MLME_DISASSOC, reason_code);
}

static void
mtlk_michael_mic_failure(struct hostapd_data *hapd, const u8 *addr)
{
	union wpa_event_data data;
	os_memset(&data, 0, sizeof(data));
	data.michael_mic_failure.unicast = 1;
	data.michael_mic_failure.src = addr;
	wpa_supplicant_event(hapd, EVENT_MICHAEL_MIC_FAILURE, &data);
}

static int
mtlk_wireless_michaelmicfailure(struct mtlk_driver_data *drv,
				const char *ev, int len)
{
	const struct iw_michaelmicfailure *mic;
	u8 *addr;

	if (len < sizeof(*mic)) {
		wpa_printf(MSG_DEBUG,
			"Invalid MIC Failure data from driver");
		return -1;
	}

	mic = (const struct iw_michaelmicfailure *) ev;

	addr = (u8*) mic->src_addr.sa_data;
	wpa_printf(MSG_DEBUG,
		"Michael MIC failure wireless event: "
		"flags=0x%x src_addr=" MACSTR, mic->flags, MAC2STR(addr));

	mtlk_michael_mic_failure(drv->hapd, addr);

	return 0;
}

static void
mtlk_wireless_event_wireless_custom(struct mtlk_driver_data *drv,
				       char *custom)
{
	const char newsta_tag[] = "NEWSTA ";
	const char rsnie_tag[]  = "RSNIE_LEN ";

	wpa_printf(MSG_DEBUG, "Custom wireless event: '%s'",
		      custom);

	if (strncmp(custom, newsta_tag, strlen(newsta_tag)) == 0) {
		char *pos = custom;
		u8 addr[ETH_ALEN];
		u8 *rsnie, ielen;
		pos += strlen(newsta_tag);
		if (hwaddr_aton(pos, addr) != 0) {
			wpa_printf(MSG_DEBUG,
				"NEWSTA with invalid MAC address");
			return;
		}
		pos = strstr(pos, rsnie_tag);
		pos += strlen(rsnie_tag);
		ielen = atoi(pos);
		if (!ielen) {
			wpa_printf(MSG_DEBUG,
				"NEWSTA with zero RSNIE length?");
			return;
		}
		rsnie = malloc(ielen);
		if (!rsnie) {
			printf("ERROR: can't allocate buffer "
				"of %i bytes for RSNIE", ielen);
			return;
		}
		pos = strstr(pos, " : ");
		pos += 3;
		hexstr2bin(pos, rsnie, ielen);
		drv_event_assoc(drv->hapd, addr, rsnie, ielen, 0);
		free(rsnie);
	} else
	if (strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		char *pos;
		u8 addr[ETH_ALEN];
		pos = strstr(custom, "addr=");
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG,
				      "MLME-MICHAELMICFAILURE.indication "
				      "without sender address ignored");
			return;
		}
		pos += 5;
		if (hwaddr_aton(pos, addr) == 0) {
			mtlk_michael_mic_failure(drv->hapd, addr);
		} else {
			wpa_printf(MSG_DEBUG,
				      "MLME-MICHAELMICFAILURE.indication "
				      "with invalid MAC address");
		}
	} else if (strncmp(custom, "STA-TRAFFIC-STAT", 16) == 0) {
		char *key, *value;
		u32 val;
		key = custom;
		while ((key = strchr(key, '\n')) != NULL) {
			key++;
			value = strchr(key, '=');
			if (value == NULL)
				continue;
			*value++ = '\0';
			val = strtoul(value, NULL, 10);
			if (strcmp(key, "mac") == 0)
				hwaddr_aton(value, drv->acct_mac);
			else if (strcmp(key, "rx_packets") == 0)
				drv->acct_data.rx_packets = val;
			else if (strcmp(key, "tx_packets") == 0)
				drv->acct_data.tx_packets = val;
			else if (strcmp(key, "rx_bytes") == 0)
				drv->acct_data.rx_bytes = val;
			else if (strcmp(key, "tx_bytes") == 0)
				drv->acct_data.tx_bytes = val;
			key = value;
		}
	}
}

static void
mtlk_wireless_event_wireless(struct mtlk_driver_data *drv,
					    char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		wpa_printf(MSG_DEBUG, "Wireless event: "
			      "cmd=0x%x len=%d", iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		iwe->u.data.pointer = custom;
		if (drv->we_version > 18 &&
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
		     iwe->cmd == IWEVCUSTOM)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN,
			       sizeof(struct iw_event) - dlen);
		} else {
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case IWEVEXPIRED:
			drv_event_disassoc(drv->hapd, (u8*)iwe->u.addr.sa_data);
			break;
		case IWEVREGISTERED:
			drv_event_assoc(drv->hapd, (u8*)iwe->u.addr.sa_data, NULL, 0, 0);
			break;
		case IWEVMICHAELMICFAILURE:
			mtlk_wireless_michaelmicfailure(drv, custom,
							iwe->u.data.length);
			break;
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end)
				return;
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;		/* XXX */
			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			mtlk_wireless_event_wireless_custom(drv, buf);
			free(buf);
			break;
		}

		pos += iwe->len;
	}
}


static void
mtlk_wireless_event_rtm_newlink(void *ctx, struct ifinfomsg *ifi,
				u8 *buf, size_t len)
{
	struct mtlk_driver_data *drv = ctx;
	int attrlen, rta_len;
	struct rtattr *attr;

	if (ifi->ifi_index != drv->ifindex)
		return;

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			mtlk_wireless_event_wireless(
				drv, ((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static int
mtlk_get_we_version(struct mtlk_driver_data *drv)
{
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	drv->we_version = 0;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = malloc(buflen);
	if (range == NULL)
		return -1;
	memset(range, 0, buflen);

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(drv->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		perror("ioctl[SIOCGIWRANGE]");
		free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->we_version = range->we_version_compiled;
	}

	free(range);
	return 0;
}


static int
mtlk_wireless_event_init(struct mtlk_driver_data *drv)
{
	struct netlink_config *cfg;

	mtlk_get_we_version(drv);

	cfg = os_zalloc(sizeof(*cfg));
	if (cfg == NULL)
		return -1;
	cfg->ctx = drv;
	cfg->newlink_cb = mtlk_wireless_event_rtm_newlink;
	drv->netlink = netlink_init(cfg);
	if (drv->netlink == NULL) {
		os_free(cfg);
		return -1;
	}

	return 0;
}


static int
mtlk_send_eapol(void *priv, const u8 *addr, const u8 *data, size_t data_len,
                int encrypt, const u8 *own_addr, u32 flags)
{
	struct mtlk_driver_data *drv = priv;
	unsigned char buf[3000];
	unsigned char *bp = buf;
	struct l2_ethhdr *eth;
	size_t len;
	int status;

	/*
	 * Prepend the Ethernet header.  If the caller left us
	 * space at the front we could just insert it but since
	 * we don't know we copy to a local buffer.  Given the frequency
	 * and size of frames this probably doesn't matter.
	 */
	len = data_len + sizeof(struct l2_ethhdr);
	if (len > sizeof(buf)) {
		bp = malloc(len);
		if (bp == NULL) {
			printf("EAPOL frame discarded, cannot malloc temp "
			       "buffer of size %lu!\n", (unsigned long) len);
			return -1;
		}
	}
	eth = (struct l2_ethhdr *) bp;
	memcpy(eth->h_dest, addr, ETH_ALEN);
	memcpy(eth->h_source, own_addr, ETH_ALEN);
	eth->h_proto = htons(ETH_P_EAPOL);
	memcpy(eth+1, data, data_len);

	status = l2_packet_send(drv->sock_xmit, addr, ETH_P_EAPOL, bp, len);

	if (bp != buf)
		free(bp);
	return status;
}

static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len)
{
	struct mtlk_driver_data *drv = ctx;

	drv_event_eapol_rx(drv->hapd, src_addr, buf + sizeof(struct l2_ethhdr),
			   len - sizeof(struct l2_ethhdr));
}

static void *
mtlk_init(struct hostapd_data *hapd, struct wpa_init_params *params)
{
	struct mtlk_driver_data *drv;
	struct ifreq ifr;
	struct iwreq iwr;

	drv = os_zalloc(sizeof(struct mtlk_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for mtlk driver data\n");
		goto bad;
	}

	drv->hapd = hapd;
	drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->ioctl_sock < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		goto bad;
	}
	memcpy(drv->iface, params->ifname, sizeof(drv->iface));

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", drv->iface);
	if (ioctl(drv->ioctl_sock, SIOCGIFINDEX, &ifr) != 0) {
		perror("ioctl(SIOCGIFINDEX)");
		goto bad;
	}
	drv->ifindex = ifr.ifr_ifindex;

	drv->sock_xmit = l2_packet_init(drv->iface, NULL, ETH_P_EAPOL,
					handle_read, drv, 1);
	if (drv->sock_xmit == NULL)
		goto bad;
	if (l2_packet_get_own_addr(drv->sock_xmit, params->own_addr))
		goto bad;
	if (params->bridge[0]) {
		wpa_printf(MSG_DEBUG,
			"Configure bridge %s for EAPOL traffic.",
			params->bridge[0]);
		drv->sock_recv = l2_packet_init(params->bridge[0], NULL,
						ETH_P_EAPOL, handle_read, drv,
						1);
		if (drv->sock_recv == NULL)
			goto bad;
	} else
		drv->sock_recv = drv->sock_xmit;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);

	iwr.u.mode = IW_MODE_MASTER;

	if (ioctl(drv->ioctl_sock, SIOCSIWMODE, &iwr) < 0) {
		perror("ioctl[SIOCSIWMODE]");
		printf("Could not set interface to master mode!\n");
		goto bad;
	}

	mtlk_set_iface_flags(drv, 0);	/* mark down during setup */

	if (mtlk_wireless_event_init(drv))
		goto bad;

	return drv;
bad:
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv != NULL)
		free(drv);
	return NULL;
}


static void
mtlk_deinit(void *priv)
{
	struct mtlk_driver_data *drv = priv;

	netlink_deinit(drv->netlink);
	(void) mtlk_set_iface_flags(drv, 0);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv->sock_recv != NULL && drv->sock_recv != drv->sock_xmit)
		l2_packet_deinit(drv->sock_recv);
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	free(drv);
}

static int
mtlk_set_ssid(void *priv, const u8 *buf, int len)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len + 1;

	if (ioctl(drv->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		printf("len=%d\n", len);
		return -1;
	}
	return 0;
}

static int
mtlk_get_ssid(void *priv, u8 *buf, int len)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len + 1;

	if (ioctl(drv->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCGIWESSID]");
		ret = -1;
	} else
		ret = iwr.u.essid.length;

	return ret;
}

static int
mtlk_commit(void *priv)
{
	return mtlk_set_iface_flags(priv, 1);
}

#ifdef CONFIG_WPS
static int
mtlk_set_wps_ie(void *priv, const u8 *ie, size_t ie_len, u16 ie_type)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) ie;
	iwr.u.data.length = ie_len;
	iwr.u.data.flags = ie_type;

	if (ioctl(drv->ioctl_sock, SIOCSIWGENIE, &iwr) < 0) {
		perror("ioctl[SIOCSIWGENIE]");
		ret = -1;
	}

	return ret;
}

static int
mtlk_set_ap_wps_ie(void *priv, const struct wpabuf *beacon,
			const struct wpabuf *proberesp,
			const struct wpabuf *assocresp)
{
	int ret;
	ret = mtlk_set_wps_ie(priv, beacon ? wpabuf_head(beacon) : NULL,
				beacon ? wpabuf_len(beacon) : 0,
				IE_WSC_BEACON);
	if (ret < 0)
		return ret;
	ret = mtlk_set_wps_ie(priv, proberesp ? wpabuf_head(proberesp) : NULL,
				proberesp ? wpabuf_len(proberesp) : 0,
				IE_WSC_PROBE_RESP);
	if (ret < 0)
		return ret;
	ret = mtlk_set_wps_ie(priv, assocresp ? wpabuf_head(assocresp) : NULL,
				assocresp ? wpabuf_len(assocresp) : 0,
				IE_WSC_ASSOC_RESP);
	return ret;
}

#else
#define mtlk_set_ap_wps_ie NULL
#endif

const struct wpa_driver_ops wpa_driver_mtlk_ops = {
	.name			= "mtlk",
	.hapd_init		= mtlk_init,
	.deinit			= mtlk_deinit,
	.set_key		= mtlk_set_encryption,
	.get_seqnum		= mtlk_get_seqnum,
	.flush			= mtlk_flush,
	.set_generic_elem	= mtlk_set_generic_elem,
	.hapd_send_eapol	= mtlk_send_eapol,
	.sta_disassoc		= mtlk_sta_disassoc,
	.sta_deauth		= mtlk_sta_deauth,
	.hapd_set_ssid		= mtlk_set_ssid,
	.hapd_get_ssid		= mtlk_get_ssid,
	.sta_clear_stats	= mtlk_sta_clear_stats,
	.commit			= mtlk_commit,
	.set_ap_wps_ie		= mtlk_set_ap_wps_ie,
};
