/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Ken Rice, Asteria Solutions Group, Inc <ken@asteriasgi.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Eliot Gable <egable AT.AT broadvox.com>
 *
 *
 * sofia_glue.c -- SOFIA SIP Endpoint (code to tie sofia to freeswitch)
 *
 */
#include "mod_sofia.h"
#include <switch_stun.h>


void sofia_glue_set_image_sdp(private_object_t *tech_pvt, switch_t38_options_t *t38_options)
{
	char buf[2048];
	const char *ip = t38_options->ip;
	uint32_t port = t38_options->port;
	const char *family = "IP4";
	const char *username = tech_pvt->profile->username;

	if (!ip) {
		if (!(ip = tech_pvt->adv_sdp_audio_ip)) {
			ip = tech_pvt->proxy_sdp_audio_ip;
		}
	}

	if (!port) {
		if (!(port = tech_pvt->adv_sdp_audio_port)) {
			port = tech_pvt->proxy_sdp_audio_port;
		}
	}

	if (!tech_pvt->owner_id) {
		tech_pvt->owner_id = (uint32_t) switch_epoch_time_now(NULL) - port;
	}

	if (!tech_pvt->session_id) {
		tech_pvt->session_id = tech_pvt->owner_id;
	}

	tech_pvt->session_id++;

	family = strchr(ip, ':') ? "IP6" : "IP4";
	switch_snprintf(buf, sizeof(buf),
					"v=0\n"
					"o=%s %010u %010u IN %s %s\n"
					"s=%s\n"
					"c=IN %s %s\n" 
					"t=0 0\n"
					"m=image %d udptl t38\n"
					"a=T38MaxBitRate:%d\n"
					"%s"
					"%s"
					"%s"
					"a=T38FaxRateManagement:%s\n"
					"a=T38FaxMaxBuffer:%d\n"
					"a=T38FaxMaxDatagram:%d\n"
					"a=T38FaxUdpEC:%s\n"
					"a=T38VendorInfo:%s\n",
					
					username,
					tech_pvt->owner_id, 
					tech_pvt->session_id,
					family,
					ip,
					username,
					family,
					ip,
					port,
					
					t38_options->T38MaxBitRate,
					t38_options->T38FaxFillBitRemoval ? "a=T38FaxFillBitRemoval\n" : "",
					t38_options->T38FaxTranscodingMMR ? "a=T38FaxTranscodingMMR\n" : "",
					t38_options->T38FaxTranscodingJBIG ? "a=T38FaxTranscodingJBIG\n" : "",
					t38_options->T38FaxRateManagement,
					t38_options->T38FaxMaxBuffer,
					t38_options->T38FaxMaxDatagram,
					t38_options->T38FaxUdpEC,
					t38_options->T38VendorInfo
					);
	
	sofia_glue_tech_set_local_sdp(tech_pvt, buf, SWITCH_TRUE);
}

void sofia_glue_set_local_sdp(private_object_t *tech_pvt, const char *ip, uint32_t port, const char *sr, int force)
{
	char buf[2048];
	int ptime = 0;
	uint32_t rate = 0;
	uint32_t v_port;
	int use_cng = 1;
	const char *val;
	const char *family;
	const char *pass_fmtp = switch_channel_get_variable(tech_pvt->channel, "sip_video_fmtp");
	const char *ov_fmtp = switch_channel_get_variable(tech_pvt->channel, "sip_force_video_fmtp");
	char srbuf[128] = "";
	const char *var_val;
	const char *username = tech_pvt->profile->username;

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) ||
		((val = switch_channel_get_variable(tech_pvt->channel, "supress_cng")) && switch_true(val)) ||
		((val = switch_channel_get_variable(tech_pvt->channel, "suppress_cng")) && switch_true(val))) {
		use_cng = 0;
		tech_pvt->cng_pt = 0;
	}

	if (!force && !ip && !sr
		&& (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) || switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA))) {
		return;
	}

	if (!ip) {
		if (!(ip = tech_pvt->adv_sdp_audio_ip)) {
			ip = tech_pvt->proxy_sdp_audio_ip;
		}
	}
	if (!port) {
		if (!(port = tech_pvt->adv_sdp_audio_port)) {
			port = tech_pvt->proxy_sdp_audio_port;
		}
	}

	if (!sr) {
		sr = "sendrecv";
	}

	if (!tech_pvt->owner_id) {
		tech_pvt->owner_id = (uint32_t) switch_epoch_time_now(NULL) - port;
	}

	if (!tech_pvt->session_id) {
		tech_pvt->session_id = tech_pvt->owner_id;
	}

	tech_pvt->session_id++;

	if ((tech_pvt->profile->ndlb & PFLAG_NDLB_SENDRECV_IN_SESSION) || 
		((var_val=switch_channel_get_variable(tech_pvt->channel, "ndlb_sendrecv_in_session")) && switch_true(var_val))) {
		switch_snprintf(srbuf, sizeof(srbuf), "a=%s\n", sr);
		sr = NULL;
	}
	
	family = strchr(ip, ':') ? "IP6" : "IP4";
	switch_snprintf(buf, sizeof(buf),
					"v=0\n"
					"o=%s %010u %010u IN %s %s\n"
					"s=%s\n"
					"c=IN %s %s\n" "t=0 0\n"
					"%sm=audio %d RTP/%sAVP",
					username,
					tech_pvt->owner_id, tech_pvt->session_id, family, ip, username, family, ip, 
					srbuf,
					port,
					(!switch_strlen_zero(tech_pvt->local_crypto_key) 
					&& sofia_test_flag(tech_pvt,TFLAG_SECURE)) ? "S" : "");

	if (tech_pvt->rm_encoding) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->pt);
	} else if (tech_pvt->num_codecs) {
		int i;
		int already_did[128] = { 0 };
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

			if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
				continue;
			}

			if (imp->ianacode < 128) {
				if (already_did[imp->ianacode]) {
					continue;
				}

				already_did[imp->ianacode] = 1;
			}

			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", imp->ianacode);
			if (!ptime) {
				ptime = imp->microseconds_per_packet / 1000;
			}
		}
	}

	if (tech_pvt->dtmf_type == DTMF_2833 && tech_pvt->te > 95) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->te);
	}

	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) && tech_pvt->cng_pt && use_cng) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->cng_pt);
	}

	switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\n");


	if (tech_pvt->rm_encoding) {
		rate = tech_pvt->rm_rate;
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%d\n", tech_pvt->agreed_pt, tech_pvt->rm_encoding, rate);
		if (tech_pvt->fmtp_out) {
			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", tech_pvt->agreed_pt, tech_pvt->fmtp_out);
		}
		if (tech_pvt->read_codec.implementation && !ptime) {
			ptime = tech_pvt->read_codec.implementation->microseconds_per_packet / 1000;
		}

	} else if (tech_pvt->num_codecs) {
		int i;
		int already_did[128] = { 0 };
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

			if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
				continue;
			}

			if (imp->ianacode < 128) {
				if (already_did[imp->ianacode]) {
					continue;
				}

				already_did[imp->ianacode] = 1;
			}

			rate = imp->samples_per_second;

			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%d\n", imp->ianacode, imp->iananame, rate);
			if (imp->fmtp) {
				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", imp->ianacode, imp->fmtp);
			}
		}
	}

	if (tech_pvt->dtmf_type == DTMF_2833 && tech_pvt->te > 95) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d telephone-event/8000\na=fmtp:%d 0-16\n", tech_pvt->te, tech_pvt->te);
	}
	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) && tech_pvt->cng_pt && use_cng) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d CN/8000\n", tech_pvt->cng_pt);
		if (!tech_pvt->rm_encoding) {
			tech_pvt->cng_pt = 0;
		}
	} else {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=silenceSupp:off - - - -\n");
	}

	if (ptime) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=ptime:%d\n", ptime);
	}

	if (sr) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=%s\n", sr);
	} 

	if (!switch_strlen_zero(tech_pvt->local_crypto_key) && sofia_test_flag(tech_pvt, TFLAG_SECURE)) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=crypto:%s\n", tech_pvt->local_crypto_key);
		//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\n");
#if 0
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "m=audio %d RTP/AVP", port);

		if (tech_pvt->rm_encoding) {
			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->pt);
		} else if (tech_pvt->num_codecs) {
			int i;
			int already_did[128] = { 0 };
			for (i = 0; i < tech_pvt->num_codecs; i++) {
				const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

				if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
					continue;
				}

				if (imp->ianacode < 128) {
					if (already_did[imp->ianacode]) {
						continue;
					}

					already_did[imp->ianacode] = 1;
				}

				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", imp->ianacode);
			}
		}

		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\na=crypto:%s\n", tech_pvt->local_crypto_key);
#endif
	}

	if (sofia_test_flag(tech_pvt, TFLAG_VIDEO)) {
		if (!switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED) && !switch_channel_test_flag(tech_pvt->channel, CF_EARLY_MEDIA) &&
			!tech_pvt->local_sdp_video_port) {
			sofia_glue_tech_choose_video_port(tech_pvt, 0);
		}

		if ((v_port = tech_pvt->adv_sdp_video_port)) {
			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "m=video %d RTP/AVP", v_port);
			
			/*****************************/
			if (tech_pvt->video_rm_encoding) {
				sofia_glue_tech_set_video_codec(tech_pvt, 0);
				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->video_agreed_pt);
			} else if (tech_pvt->num_codecs) {
				int i;
				int already_did[128] = { 0 };
				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					if (imp->ianacode < 128) {
						if (already_did[imp->ianacode]) {
							continue;
						}
						already_did[imp->ianacode] = 1;
					}

					switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", imp->ianacode);
					if (!ptime) {
						ptime = imp->microseconds_per_packet / 1000;
					}
				}
			}

			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\n");

			if (tech_pvt->video_rm_encoding) {
				const char *of;
				rate = tech_pvt->video_rm_rate;
				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%ld\n", tech_pvt->video_pt, tech_pvt->video_rm_encoding,
								tech_pvt->video_rm_rate);

				pass_fmtp = NULL;

				if (switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE)) {
					if ((of = switch_channel_get_variable_partner(tech_pvt->channel, "sip_video_fmtp"))) {
						pass_fmtp = of;
					}
				}

				if (ov_fmtp) {
					pass_fmtp = ov_fmtp;
				}

				if (pass_fmtp) {
					switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", tech_pvt->video_pt, pass_fmtp);
				}

			} else if (tech_pvt->num_codecs) {
				int i;
				int already_did[128] = { 0 };

				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					if (imp->ianacode < 128) {
						if (already_did[imp->ianacode]) {
							continue;
						}
						already_did[imp->ianacode] = 1;
					}

					if (!rate) {
						rate = imp->samples_per_second;
					}

					switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%d\n", imp->ianacode, imp->iananame,
									imp->samples_per_second);
					if (imp->fmtp) {
						switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", imp->ianacode, imp->fmtp);
					} else {
						if (pass_fmtp) {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", imp->ianacode, pass_fmtp);
						}
					}
				}
			}
		}
	}
	sofia_glue_tech_set_local_sdp(tech_pvt, buf, SWITCH_TRUE);
}

void sofia_glue_tech_prepare_codecs(private_object_t *tech_pvt)
{
	const char *abs, *codec_string = NULL;
	const char *ocodec = NULL;

	if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) || switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
		return;
	}

	if (tech_pvt->num_codecs) {
		return;
	}

	switch_assert(tech_pvt->session != NULL);

	if ((abs = switch_channel_get_variable(tech_pvt->channel, "absolute_codec_string"))) {
		codec_string = abs;
	} else {
		if (!(codec_string = switch_channel_get_variable(tech_pvt->channel, "codec_string"))) {
			if (tech_pvt->profile->codec_string) {
				codec_string = tech_pvt->profile->codec_string;
			}
		}

		if ((ocodec = switch_channel_get_variable(tech_pvt->channel, SWITCH_ORIGINATOR_CODEC_VARIABLE))) {
			if (!codec_string || sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_TRANSCODING)) {
				codec_string = ocodec;
			} else {
				if (!(codec_string = switch_core_session_sprintf(tech_pvt->session, "%s,%s", ocodec, codec_string))) {
					codec_string = ocodec;
				}
			}
		}
	}


	if (codec_string) {
		char *tmp_codec_string;
		if ((tmp_codec_string = switch_core_session_strdup(tech_pvt->session, codec_string))) {
			tech_pvt->codec_order_last = switch_separate_string(tmp_codec_string, ',', tech_pvt->codec_order, SWITCH_MAX_CODECS);
			tech_pvt->num_codecs =
				switch_loadable_module_get_codecs_sorted(tech_pvt->codecs, SWITCH_MAX_CODECS, tech_pvt->codec_order, tech_pvt->codec_order_last);
		}
	} else {
		tech_pvt->num_codecs = switch_loadable_module_get_codecs(tech_pvt->codecs, sizeof(tech_pvt->codecs) / sizeof(tech_pvt->codecs[0]));
	}


}

void sofia_glue_check_video_codecs(private_object_t *tech_pvt)
{
	if (tech_pvt->num_codecs && !sofia_test_flag(tech_pvt, TFLAG_VIDEO)) {
		int i;
		tech_pvt->video_count = 0;
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			if (tech_pvt->codecs[i]->codec_type == SWITCH_CODEC_TYPE_VIDEO) {
				tech_pvt->video_count++;
			}
		}
		if (tech_pvt->video_count) {
			sofia_set_flag_locked(tech_pvt, TFLAG_VIDEO);
		}
	}
}


void sofia_glue_attach_private(switch_core_session_t *session, sofia_profile_t *profile, private_object_t *tech_pvt, const char *channame)
{
	char name[256];
	unsigned int x;
	char *p;

	switch_assert(session != NULL);
	switch_assert(profile != NULL);
	switch_assert(tech_pvt != NULL);

	switch_core_session_add_stream(session, NULL);

	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_mutex_lock(profile->flag_mutex);

	/* copy flags from profile to the sofia private */
	for(x = 0; x < TFLAG_MAX; x++ ) {
		tech_pvt->flags[x] = profile->flags[x];
	}

	tech_pvt->x_freeswitch_support_local = FREESWITCH_SUPPORT;

	tech_pvt->profile = profile;
	profile->inuse++;
	switch_mutex_unlock(profile->flag_mutex);
	switch_mutex_unlock(tech_pvt->flag_mutex);

	if (tech_pvt->bte) {
		tech_pvt->te = tech_pvt->bte;
	} else if (!tech_pvt->te) {
		tech_pvt->te = profile->te;
	}

	tech_pvt->dtmf_type = profile->dtmf_type;

	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG)) {
		if (tech_pvt->bcng_pt) {
			tech_pvt->cng_pt = tech_pvt->bcng_pt;
		} else if (!tech_pvt->cng_pt) {
			tech_pvt->cng_pt = profile->cng_pt;
		}
	}

	tech_pvt->session = session;
	tech_pvt->channel = switch_core_session_get_channel(session);
	switch_channel_set_cap(tech_pvt->channel, CC_MEDIA_ACK);
	switch_channel_set_cap(tech_pvt->channel, CC_BYPASS_MEDIA);
	switch_channel_set_cap(tech_pvt->channel, CC_PROXY_MEDIA);
	
	switch_core_session_set_private(session, tech_pvt);

	switch_snprintf(name, sizeof(name), "sofia/%s/%s", profile->name, channame);
	if ((p = strchr(name, ';'))) {
		*p = '\0';
	}
	switch_channel_set_name(tech_pvt->channel, name);
}

switch_status_t sofia_glue_ext_address_lookup(sofia_profile_t *profile, private_object_t *tech_pvt, char **ip, switch_port_t *port, const char *sourceip, switch_memory_pool_t *pool)
{
	char *error = "";
	switch_status_t status = SWITCH_STATUS_FALSE;
	int x;
	switch_port_t myport = *port;
	const char *var;
	int funny = 0;
	switch_port_t stun_port = SWITCH_STUN_DEFAULT_PORT;
	char *stun_ip = NULL;

	if (!sourceip) {
		return status;
	}

	if (!strncasecmp(sourceip, "host:", 5)) {
		status = (*ip = switch_stun_host_lookup(sourceip + 5, pool)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
	} else if (!strncasecmp(sourceip, "stun:", 5)) {
		char *p;

		if (!sofia_test_pflag(profile, PFLAG_STUN_ENABLED)) {
			*ip = switch_core_strdup(pool, profile->rtpip);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Trying to use STUN but its disabled!\n");
			goto out;
		}
		
		stun_ip = strdup(sourceip + 5);

		if ((p = strchr(stun_ip, ':'))) {
			int iport;
			*p++ = '\0';
			iport = atoi(p);
			if (iport > 0 && iport < 0xFFFF) {
				stun_port = (switch_port_t) iport;
			}
		}

		if (switch_strlen_zero(stun_ip)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! NO STUN SERVER\n");
			goto out;
		}

		for (x = 0; x < 5; x++) {
			if (sofia_test_pflag(profile, PFLAG_FUNNY_STUN) || 
				(tech_pvt && (var = switch_channel_get_variable(tech_pvt->channel, "funny_stun")) && switch_true(var))) {
				error = "funny";
				funny++;
			}
			if ((status = switch_stun_lookup(ip, port, stun_ip, stun_port, &error, pool)) != SWITCH_STATUS_SUCCESS) {
				switch_yield(100000);
			} else {
				break;
			}
		}
		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! %s:%d [%s]\n", stun_ip, stun_port, error);
			goto out;
		}
		if (!*ip) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! No IP returned\n");
			goto out;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN Success [%s]:[%d]\n", *ip, *port);
		status = SWITCH_STATUS_SUCCESS;
		if (tech_pvt) {
			if (myport == *port && !strcmp(*ip, tech_pvt->profile->rtpip)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN Not Required ip and port match. [%s]:[%d]\n", *ip, *port);
				if (sofia_test_pflag(profile, PFLAG_STUN_AUTO_DISABLE)) {
					sofia_clear_pflag(profile, PFLAG_STUN_ENABLED);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN completely disabled.\n");
				}
			} else {
				tech_pvt->stun_ip = switch_core_session_strdup(tech_pvt->session, stun_ip);
				tech_pvt->stun_port = stun_port;
				tech_pvt->stun_flags |= STUN_FLAG_SET;
				if (funny) {
					tech_pvt->stun_flags |= STUN_FLAG_FUNNY;
				}
			}
		}
	} else {
		*ip = (char*)sourceip;
		status = SWITCH_STATUS_SUCCESS;
	}

 out:

	switch_safe_free(stun_ip);

	return status;
}


const char *sofia_glue_get_unknown_header(sip_t const *sip, const char *name)
{
	sip_unknown_t *un;
	for (un = sip->sip_unknown; un; un = un->un_next) {
		if (!strcasecmp(un->un_name, name)) {
			if (!switch_strlen_zero(un->un_value)) {
				return un->un_value;
			}
		}
	}
	return NULL;
}

switch_status_t sofia_glue_tech_choose_port(private_object_t *tech_pvt, int force)
{
	char *lookup_rtpip = tech_pvt->profile->rtpip;	/* Pointer to externally looked up address */
	switch_port_t sdp_port;		/* The external port to be sent in the SDP */
	const char *use_ip = NULL;	/* The external IP to be sent in the SDP */

	/* Don't do anything if we're in proxy mode or if a (remote) port already has been found */
	if (!force) {
		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) ||
			switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) || tech_pvt->adv_sdp_audio_port) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	/* Release the local sdp port */
	if (tech_pvt->local_sdp_audio_port) {
		switch_rtp_release_port(tech_pvt->profile->rtpip, tech_pvt->local_sdp_audio_port);
	}

	/* Request a local port from the core's allocator */
	if (!(tech_pvt->local_sdp_audio_port = switch_rtp_request_port(tech_pvt->profile->rtpip))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_CRIT, "No RTP ports available!\n");
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->local_sdp_audio_ip = tech_pvt->profile->rtpip;
	
	sdp_port = tech_pvt->local_sdp_audio_port;

	if (!(use_ip = switch_channel_get_variable(tech_pvt->channel, "rtp_adv_audio_ip")) 
		&& !switch_strlen_zero(tech_pvt->profile->extrtpip)) {
			use_ip = tech_pvt->profile->extrtpip;
	}

	if (use_ip) {
		if (sofia_glue_ext_address_lookup(tech_pvt->profile, tech_pvt, &lookup_rtpip, &sdp_port, 
										  use_ip, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			/* Address lookup was required and fail (external ip was "host:..." or "stun:...") */
			return SWITCH_STATUS_FALSE;	
		} else {
			if (lookup_rtpip == use_ip) {
				/* sofia_glue_ext_address_lookup didn't return any error, but the return IP is the same as the original one, 
				which means no lookup was necessary. Check if NAT is detected  */
				if (sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
					/* Yes, map the port through switch_nat */
					switch_nat_add_mapping(tech_pvt->local_sdp_audio_port, SWITCH_NAT_UDP, &sdp_port, SWITCH_FALSE);
				} else {
					/* No NAT detected */
					use_ip = tech_pvt->profile->rtpip;
				}
			} else {
				/* Address properly resolved, use it as external ip */ 
				use_ip = lookup_rtpip;
			}
		}
	} else {
		/* No NAT traversal required, use the profile's rtp ip */
		use_ip = tech_pvt->profile->rtpip;
	}
	
	tech_pvt->adv_sdp_audio_port = sdp_port;
	tech_pvt->adv_sdp_audio_ip = tech_pvt->extrtpip = switch_core_session_strdup(tech_pvt->session, use_ip);

	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable_printf(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, "%d", sdp_port);

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t sofia_glue_tech_choose_video_port(private_object_t *tech_pvt, int force)
{
	char *lookup_rtpip = tech_pvt->profile->rtpip;	/* Pointer to externally looked up address */
	switch_port_t sdp_port;		/* The external port to be sent in the SDP */
	const char *use_ip = NULL;	/* The external IP to be sent in the SDP */

	/* Don't do anything if we're in proxy mode or if a (remote) port already has been found */
	if (!force) {
		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) ||
			switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) || tech_pvt->adv_sdp_video_port) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	/* Release the local sdp port */
	if (tech_pvt->local_sdp_video_port) {
		switch_rtp_release_port(tech_pvt->profile->rtpip, tech_pvt->local_sdp_video_port);
	}

	/* Request a local port from the core's allocator */
	if (!(tech_pvt->local_sdp_video_port = switch_rtp_request_port(tech_pvt->profile->rtpip))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_CRIT, "No RTP ports available!\n");
		return SWITCH_STATUS_FALSE;
	}

	sdp_port = tech_pvt->local_sdp_video_port;

	if (!(use_ip = switch_channel_get_variable(tech_pvt->channel, "rtp_adv_video_ip")) 
		&& !switch_strlen_zero(tech_pvt->profile->extrtpip)) {
			use_ip = tech_pvt->profile->extrtpip;
	}

	if (use_ip) {
		if (sofia_glue_ext_address_lookup(tech_pvt->profile, tech_pvt, &lookup_rtpip, &sdp_port, 
										  use_ip, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			/* Address lookup was required and fail (external ip was "host:..." or "stun:...") */
			return SWITCH_STATUS_FALSE;	
		} else {
			if (lookup_rtpip == use_ip) {
				/* sofia_glue_ext_address_lookup didn't return any error, but the return IP is the same as the original one, 
				which means no lookup was necessary. Check if NAT is detected  */
				if (sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
					/* Yes, map the port through switch_nat */
					switch_nat_add_mapping(tech_pvt->local_sdp_video_port, SWITCH_NAT_UDP, &sdp_port, SWITCH_FALSE);
				} else {
					/* No NAT detected */
					use_ip = tech_pvt->profile->rtpip;
				}
			} else {
				/* Address properly resolved, use it as external ip */ 
				use_ip = lookup_rtpip;
			}
		}
	} else {
		/* No NAT traversal required, use the profile's rtp ip */
		use_ip = tech_pvt->profile->rtpip;
	}
	
	tech_pvt->adv_sdp_video_port = sdp_port;
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable_printf(tech_pvt->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE, "%d", sdp_port);

	return SWITCH_STATUS_SUCCESS;
}

sofia_transport_t sofia_glue_str2transport(const char *str)
{
	if (!strncasecmp(str, "udp", 3)) {
		return SOFIA_TRANSPORT_UDP;
	} else if (!strncasecmp(str, "tcp", 3)) {
		return SOFIA_TRANSPORT_TCP;
	} else if (!strncasecmp(str, "sctp", 4)) {
		return SOFIA_TRANSPORT_SCTP;
	} else if (!strncasecmp(str, "tls", 3)) {
		return SOFIA_TRANSPORT_TCP_TLS;
	}

	return SOFIA_TRANSPORT_UNKNOWN;
}

char *sofia_glue_find_parameter(const char *str, const char *param)
{
	char *ptr = NULL;

	ptr = (char *) str;
	while (ptr) {
		if (!strncasecmp(ptr, param, strlen(param)))
			return ptr;

		if ((ptr = strchr(ptr, ';')))
			ptr++;
	}

	return NULL;
}

sofia_transport_t sofia_glue_url2transport(const url_t *url)
{
	char *ptr = NULL;
	int tls = 0;

	if (!url)
		return SOFIA_TRANSPORT_UNKNOWN;

	if (url->url_scheme && !strcasecmp(url->url_scheme, "sips")) {
		tls++;
	}

	if ((ptr = sofia_glue_find_parameter(url->url_params, "transport="))) {
		return sofia_glue_str2transport(ptr + 10);
	}

	return (tls) ? SOFIA_TRANSPORT_TCP_TLS : SOFIA_TRANSPORT_UDP;
}

sofia_transport_t sofia_glue_via2transport(const sip_via_t *via)
{
	char *ptr = NULL;

	if (!via || !via->v_protocol)
		return SOFIA_TRANSPORT_UNKNOWN;

	if ((ptr = strrchr(via->v_protocol, '/'))) {
		ptr++;

		if (!strncasecmp(ptr, "udp", 3)) {
			return SOFIA_TRANSPORT_UDP;
		} else if (!strncasecmp(ptr, "tcp", 3)) {
			return SOFIA_TRANSPORT_TCP;
		} else if (!strncasecmp(ptr, "tls", 3)) {
			return SOFIA_TRANSPORT_TCP_TLS;
		} else if (!strncasecmp(ptr, "sctp", 4)) {
			return SOFIA_TRANSPORT_SCTP;
		}
	}

	return SOFIA_TRANSPORT_UNKNOWN;
}

const char *sofia_glue_transport2str(const sofia_transport_t tp)
{
	switch (tp) {
	case SOFIA_TRANSPORT_TCP:
		return "tcp";

	case SOFIA_TRANSPORT_TCP_TLS:
		return "tls";

	case SOFIA_TRANSPORT_SCTP:
		return "sctp";

	default:
		return "udp";
	}
}

char *sofia_glue_create_external_via(switch_core_session_t *session, sofia_profile_t *profile, sofia_transport_t transport) 
{
	return sofia_glue_create_via(session,
								 profile->extsipip,
								 (sofia_glue_transport_has_tls(transport))
								 ? profile->tls_sip_port : profile->sip_port,
								 transport);
}

char *sofia_glue_create_via(switch_core_session_t *session, const char *ip, switch_port_t port, sofia_transport_t transport) 
{
	if (port && port != 5060) {
		if (session) {
			return switch_core_session_sprintf(session, "SIP/2.0/%s %s:%d;rport", sofia_glue_transport2str(transport), ip, port);		
		} else {
			return switch_mprintf("SIP/2.0/%s %s:%d;rport", sofia_glue_transport2str(transport), ip, port);		
		}
	} else {
		if (session) {
			return switch_core_session_sprintf(session, "SIP/2.0/%s %s;rport", sofia_glue_transport2str(transport), ip);
		} else {
			return switch_mprintf("SIP/2.0/%s %s;rport", sofia_glue_transport2str(transport), ip);
		}
	}
}

char *sofia_glue_strip_uri(const char *str)
{
	char *p;
	char *r;

	if ((p = strchr(str, '<'))) {
		p++;
		r = strdup(p);
		if ((p = strchr(r, '>'))) {
			*p = '\0';
		}
	} else {
		r = strdup(str);
	}

	return r;
}

int sofia_glue_check_nat(sofia_profile_t *profile, const char *network_ip)
{
	return (network_ip && 
			profile->local_network && 
			sofia_test_pflag(profile, PFLAG_AUTO_NAT) && 
			!switch_check_network_list_ip(network_ip, profile->local_network));
}

int sofia_glue_transport_has_tls(const sofia_transport_t tp)
{
	switch (tp) {
	case SOFIA_TRANSPORT_TCP_TLS:
		return 1;

	default:
		return 0;
	}
}

void sofia_glue_get_addr(msg_t *msg, char *buf, size_t buflen, int *port) {
	su_addrinfo_t *addrinfo = msg_addrinfo(msg);

	if (buf) {
		get_addr(buf, buflen, addrinfo->ai_addr, addrinfo->ai_addrlen);
	}

	if (port) {
		*port = get_port(addrinfo->ai_addr); 
	}
}

char *sofia_overcome_sip_uri_weakness(switch_core_session_t *session, const char *uri, const sofia_transport_t transport, switch_bool_t uri_only,
									  const char *params)
{
	char *stripped = switch_core_session_strdup(session, uri);
	char *new_uri = NULL;
	char *p;

	stripped = sofia_glue_get_url_from_contact(stripped, 0);

	/* remove our params so we don't make any whiny moronic device piss it's pants and forget who it is for a half-hour */
	if ((p = (char *)switch_stristr(";fs_", stripped))) {
		*p = '\0'; 
	}

	if (transport && transport != SOFIA_TRANSPORT_UDP) {

		if (switch_stristr("port=", stripped)) {
			new_uri = switch_core_session_sprintf(session, "%s%s%s", uri_only ? "" : "<", stripped, uri_only ? "" : ">");
		} else {

			if (strchr(stripped, ';')) {
				if (params) {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s;%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), params, uri_only ? "" : ">");
				} else {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), uri_only ? "" : ">");
				}
			} else {
				if (params) {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s;%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), params, uri_only ? "" : ">");
				} else {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), uri_only ? "" : ">");
				}
			}
		}
	} else {
		if (params) {
			new_uri = switch_core_session_sprintf(session, "%s%s;%s%s", uri_only ? "" : "<", stripped, params, uri_only ? "" : ">");
		} else {
			if (uri_only) {
				new_uri = stripped;
			} else {
				new_uri = switch_core_session_sprintf(session, "<%s>", stripped);
			}
		}
	}

	return new_uri;
}

#define RA_PTR_LEN 512
switch_status_t sofia_glue_tech_proxy_remote_addr(private_object_t *tech_pvt)
{
	const char *err;
	char rip[RA_PTR_LEN] = "";
	char rp[RA_PTR_LEN] = "";
	char rvp[RA_PTR_LEN] = "";
	char *p, *ip_ptr = NULL, *port_ptr = NULL, *vid_port_ptr = NULL, *pe;
	int x;
	const char *val;
	
	if (switch_strlen_zero(tech_pvt->remote_sdp_str)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((p = (char *) switch_stristr("c=IN IP4 ", tech_pvt->remote_sdp_str)) ||
		(p = (char *) switch_stristr("c=IN IP6 ", tech_pvt->remote_sdp_str))) {
		ip_ptr = p + 9;
	}

	if ((p = (char *) switch_stristr("m=audio ", tech_pvt->remote_sdp_str))) {
		port_ptr = p + 8;
	}

	if ((p = (char *) switch_stristr("m=image ", tech_pvt->remote_sdp_str))) {
		port_ptr = p + 8;
	}

	if ((p = (char *) switch_stristr("m=video ", tech_pvt->remote_sdp_str))) {
		vid_port_ptr = p + 8;
	}

	if (!(ip_ptr && port_ptr)) {
		return SWITCH_STATUS_FALSE;
	}

	p = ip_ptr;
	pe = p + strlen(p);
	x = 0;
	while (x < sizeof(rip) - 1 && p && *p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ':' || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) {
		rip[x++] = *p;
		p++;
		if (p >= pe) {
			return SWITCH_STATUS_FALSE;
		}
	}

	p = port_ptr;
	x = 0;
	while (x < sizeof(rp) - 1 && p && *p && (*p >= '0' && *p <= '9')) {
		rp[x++] = *p;
		p++;
		if (p >= pe) {
			return SWITCH_STATUS_FALSE;
		}
	}

	p = vid_port_ptr;
	x = 0;
	while (x < sizeof(rvp) - 1 && p && *p && (*p >= '0' && *p <= '9')) {
		rvp[x++] = *p;
		p++;
		if (p >= pe) {
			return SWITCH_STATUS_FALSE;
		}
	}

	if (!(*rip && *rp)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "invalid SDP\n");
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, rip);
	tech_pvt->remote_sdp_audio_port = (switch_port_t) atoi(rp);
	
	if (*rvp) {
		tech_pvt->remote_sdp_video_ip = switch_core_session_strdup(tech_pvt->session, rip);
		tech_pvt->remote_sdp_video_port = (switch_port_t) atoi(rvp);
	}

	if (tech_pvt->remote_sdp_video_ip && tech_pvt->remote_sdp_video_port) {
		if (!strcmp(tech_pvt->remote_sdp_video_ip, rip) && atoi(rvp) == tech_pvt->remote_sdp_video_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Remote video address:port [%s:%d] has not changed.\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
		} else {
			sofia_set_flag_locked(tech_pvt, TFLAG_VIDEO);
			switch_channel_set_flag(tech_pvt->channel, CF_VIDEO);
			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				if (switch_rtp_set_remote_address(tech_pvt->video_rtp_session, tech_pvt->remote_sdp_video_ip, 
												  tech_pvt->remote_sdp_video_port, SWITCH_TRUE, &err) !=
					SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", err);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "VIDEO RTP CHANGING DEST TO: [%s:%d]\n",
									  tech_pvt->remote_sdp_video_ip, tech_pvt->remote_sdp_video_port);
					if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) &&
						!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
						/* Reactivate the NAT buster flag. */
						switch_rtp_set_flag(tech_pvt->video_rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					}
				}
			}
		}
	}

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		char *remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);
		
		if (remote_host && remote_port && !strcmp(remote_host, tech_pvt->remote_sdp_audio_ip) && remote_port == tech_pvt->remote_sdp_audio_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Remote address:port [%s:%d] has not changed.\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
			return SWITCH_STATUS_SUCCESS;
		}
		
		if (switch_rtp_set_remote_address(tech_pvt->rtp_session, tech_pvt->remote_sdp_audio_ip, 
										  tech_pvt->remote_sdp_audio_port, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "AUDIO RTP CHANGING DEST TO: [%s:%d]\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				/* Reactivate the NAT buster flag. */
				switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);	
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


void sofia_glue_tech_patch_sdp(private_object_t *tech_pvt)
{
	switch_size_t len;
	char *p, *q, *pe , *qe;
	int has_video=0,has_audio=0,has_ip=0;
	char port_buf[25] = "";
	char vport_buf[25] = "";
	char *new_sdp;
	int bad = 0;

	if (switch_strlen_zero(tech_pvt->local_sdp_str)) {
		return;
	}

	len = strlen(tech_pvt->local_sdp_str) * 2;
	
	if (switch_stristr("sendonly", tech_pvt->local_sdp_str) || switch_stristr("0.0.0.0", tech_pvt->local_sdp_str)) {
	    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Skip patch on hold SDP\n");
	    return;
	}
	
	if (switch_strlen_zero(tech_pvt->adv_sdp_audio_ip) || !tech_pvt->adv_sdp_audio_port) {
	    if (sofia_glue_tech_choose_port(tech_pvt, 1) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "%s I/O Error\n", switch_channel_get_name(tech_pvt->channel));
			return;
		}
		tech_pvt->iananame = switch_core_session_strdup(tech_pvt->session, "PROXY");
		tech_pvt->rm_rate = 8000;
		tech_pvt->codec_ms = 20;
	}
	
	new_sdp = switch_core_session_alloc(tech_pvt->session, len);
	switch_snprintf(port_buf, sizeof(port_buf), "%u", tech_pvt->adv_sdp_audio_port);
	

	p = tech_pvt->local_sdp_str;
	q = new_sdp;
	pe = p + strlen(p);
	qe = q + len - 1;

	while(p && *p) {
		if (p >= pe) {
			bad = 1;
			goto end;
		}
		
		if (q >= qe) {
			bad = 2;
			goto end;
		}

	    if (tech_pvt->adv_sdp_audio_ip && !strncmp("c=IN IP", p, 7)) {
			strncpy(q, p, 9);
			p += 9;
			q += 9;
			strncpy(q, tech_pvt->adv_sdp_audio_ip, strlen(tech_pvt->adv_sdp_audio_ip));
			q += strlen(tech_pvt->adv_sdp_audio_ip);

			while(p && *p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ':' || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))) {
				if (p >= pe) {
					bad = 3;
					goto end;
				}
				p++;
			}

	    	has_ip++;

		} else if (!strncmp("m=audio ", p, 8) || (!strncmp("m=image ", p, 8))) {
			strncpy(q, p, 8);
			p += 8;

			if (p >= pe) {
				bad = 4;
				goto end;
			}


			q += 8;
			
			if (q >= qe) {
				bad = 5;
				goto end;
			}


			strncpy(q, port_buf, strlen(port_buf));
			q += strlen(port_buf);

			if (q >= qe) {
				bad = 6;
				goto end;
			}

			while (p && *p && (*p >= '0' && *p <= '9')) {
				if (p >= pe) {
					bad = 7;
					goto end;
				}
				p++;
			}

			has_audio++;		

	    } else if (!strncmp("m=video ", p, 8)) {
			if (!has_video) {
				sofia_glue_tech_choose_video_port(tech_pvt, 1);
				tech_pvt->video_rm_encoding = "PROXY-VID";
				tech_pvt->video_rm_rate = 90000;
				tech_pvt->video_codec_ms = 0;
				switch_snprintf(vport_buf, sizeof(vport_buf), "%u", tech_pvt->adv_sdp_video_port);
			}

			strncpy(q, p, 8);
			p += 8;

			if (p >= pe) {
				bad = 8;
				goto end;
			}

			q += 8;

			if (q >= qe) {
				bad = 9;
				goto end;
			}

			strncpy(q, vport_buf, strlen(vport_buf));
			q += strlen(vport_buf);

			if (q >= qe) {
				bad = 10;
				goto end;
			}

			while (p && *p && (*p >= '0' && *p <= '9')) {

				if (p >= pe) {
					bad = 11;
					goto end;
				}

				p++;
			}
			
			has_video++;
	    }
	    
	    while (p && *p && *p != '\n') {

			if (p >= pe) {
				bad = 12;
				goto end;
			}

			if (q >= qe) {
				bad = 13;
				goto end;
			}

			*q++ = *p++;
	    }
		
		if (p >= pe) {
            bad = 14;
            goto end;
        }
		
		if (q >= qe) {
			bad = 15;
			goto end;
		}

	    *q++ = *p++;

	}

 end:

	if (bad) {
		return;
	}


	if (switch_channel_down(tech_pvt->channel) || sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s too late.\n", switch_channel_get_name(tech_pvt->channel));
		return;
	}


	if (!has_ip && !has_audio) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s SDP has no audio in it.\n%s\n",
						  switch_channel_get_name(tech_pvt->channel), tech_pvt->local_sdp_str);
		return;
	}
	

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s Patched SDP\n---\n%s\n+++\n%s\n",
					  switch_channel_get_name(tech_pvt->channel), tech_pvt->local_sdp_str, new_sdp);

	sofia_glue_tech_set_local_sdp(tech_pvt, new_sdp, SWITCH_FALSE);

}


void sofia_glue_tech_set_local_sdp(private_object_t *tech_pvt, const char *sdp_str, switch_bool_t dup)
{
	switch_mutex_lock(tech_pvt->sofia_mutex);
	tech_pvt->local_sdp_str = dup ? switch_core_session_strdup(tech_pvt->session, sdp_str) : (char *)sdp_str;
	switch_mutex_unlock(tech_pvt->sofia_mutex);	
}

char* sofia_glue_get_extra_headers(switch_channel_t *channel, const char *prefix) 
{
	char *extra_headers = NULL;
	switch_stream_handle_t stream = { 0 };
    switch_event_header_t *hi = NULL;

    SWITCH_STANDARD_STREAM(stream);
    if ((hi = switch_channel_variable_first(channel))) {
        for (; hi; hi = hi->next) {
            const char *name = (char *) hi->name;
            char *value = (char *) hi->value;

            if (!strncasecmp(name, prefix, strlen(prefix))) {
                const char *hname = name + strlen(prefix);
                stream.write_function(&stream, "%s: %s\r\n", hname, value);
            }
        }
        switch_channel_variable_last(channel);
    }

    if (!switch_strlen_zero((char*)stream.data)) {
        extra_headers = stream.data;
    } else {
		switch_safe_free(stream.data);
	}

	return extra_headers;
}

void sofia_glue_set_extra_headers(switch_channel_t *channel, sip_t const *sip, const char *prefix)
{
	sip_unknown_t *un;
	char name[512] = "";
	
	if (!sip || !channel) {
		return;
	}

	for (un = sip->sip_unknown; un; un = un->un_next) {
		if (!strncasecmp(un->un_name, "X-", 2) || !strncasecmp(un->un_name, "P-", 2)) {
			if (!switch_strlen_zero(un->un_value)) {
				switch_snprintf(name, sizeof(name), "%s%s", prefix, un->un_name);
				switch_channel_set_variable(channel, name, un->un_value);
			}
		}
	}
}


switch_status_t sofia_glue_do_invite(switch_core_session_t *session)
{
	char *alert_info = NULL;
	const char *max_forwards = NULL;
	const char *alertbuf;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	const char *cid_name, *cid_num;
	char *e_dest = NULL;
	const char *holdstr = "";
	char *extra_headers = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t session_timeout = 0;
	const char *val;
	const char *rep;
	const char *call_id = NULL;
	char *route = NULL;
	char *route_uri = NULL;
	sofia_destination_t *dst = NULL;
	sofia_cid_type_t cid_type = tech_pvt->profile->cid_type;

	rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);

	switch_assert(tech_pvt != NULL);

	sofia_clear_flag_locked(tech_pvt, TFLAG_SDP);

	caller_profile = switch_channel_get_caller_profile(channel);

	cid_name = caller_profile->caller_id_name;
	cid_num = caller_profile->caller_id_number;
	sofia_glue_tech_prepare_codecs(tech_pvt);
	sofia_glue_check_video_codecs(tech_pvt);
	check_decode(cid_name, session);
	check_decode(cid_num, session);

	if (!tech_pvt->from_str) {
		const char* sipip;
		const char* format;
		const char *alt = NULL;

		if (sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
			sipip = tech_pvt->profile->extsipip;
		} else {
			sipip = tech_pvt->profile->extsipip ? tech_pvt->profile->extsipip : tech_pvt->profile->sipip;
		}

		format = strchr(sipip, ':') ? "\"%s\" <sip:%s%s[%s]>" : "\"%s\" <sip:%s%s%s>";

		if ((alt = switch_channel_get_variable(channel, "sip_invite_domain"))) {
			sipip = alt;
		}
			
		tech_pvt->from_str = 
			switch_core_session_sprintf(tech_pvt->session,
										format,
										cid_name,
										cid_num,
										!switch_strlen_zero(cid_num) ? "@" : "",
										sipip);
	}

	if ((alertbuf = switch_channel_get_variable(channel, "alert_info"))) {
		alert_info = switch_core_session_sprintf(tech_pvt->session, "Alert-Info: %s", alertbuf);
	}

	max_forwards = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE);

	if ((status = sofia_glue_tech_choose_port(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 0);

	sofia_set_flag_locked(tech_pvt, TFLAG_READY);

	if (!tech_pvt->nh) {
		char *d_url = NULL, *url = NULL;
		sofia_private_t *sofia_private;
		char *invite_contact = NULL, *to_str, *use_from_str, *from_str, *url_str;
		const char *t_var;
		char *rpid_domain = "cluecon.com", *p;
		const char *priv = "off";
		const char *screen = "no";
		const char *invite_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_params");
		const char *invite_to_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_to_params");
		const char *invite_to_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_to_uri");
		const char *invite_contact_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_contact_params");
		const char *invite_from_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_from_params");
		const char *from_var = switch_channel_get_variable(tech_pvt->channel, "sip_from_uri");
		const char *from_display = switch_channel_get_variable(tech_pvt->channel, "sip_from_display");
		
		if (switch_strlen_zero(tech_pvt->dest)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "URL Error!\n");
			return SWITCH_STATUS_FALSE;
		}

		if ((d_url = sofia_glue_get_url_from_contact(tech_pvt->dest, 1))) {
			url = d_url;
		} else {
			url = tech_pvt->dest;
		}

		url_str = url;

		if (from_var) {
			if (strncasecmp(from_var, "sip:", 4) || strncasecmp(from_var, "sips:", 5)) {
				use_from_str = switch_core_session_strdup(tech_pvt->session, from_var);
			} else {
				use_from_str = switch_core_session_sprintf(tech_pvt->session, "sip:%s", from_var);
			}
		} else if (!switch_strlen_zero(tech_pvt->gateway_from_str)) {
			use_from_str = tech_pvt->gateway_from_str;
		} else {
			use_from_str = tech_pvt->from_str;
		}

		if (!switch_strlen_zero(tech_pvt->gateway_from_str)) {
			rpid_domain = switch_core_session_strdup(session, tech_pvt->gateway_from_str);
		} else if (!switch_strlen_zero(tech_pvt->from_str)) {
			rpid_domain = switch_core_session_strdup(session, tech_pvt->from_str);
		} 

		sofia_glue_get_url_from_contact(rpid_domain, 0);
		if ((rpid_domain = strrchr(rpid_domain, '@'))) {
			rpid_domain++;
			if ((p = strchr(rpid_domain, ';'))) {
				*p = '\0';
			}
		}

		if (!rpid_domain) {
			rpid_domain = "cluecon.com";
		}

		/*
		 * Ignore transport chanvar and uri parameter for gateway connections
		 * since all of them have been already taken care of in mod_sofia.c:sofia_outgoing_channel()
		 */
		if (tech_pvt->transport == SOFIA_TRANSPORT_UNKNOWN && switch_strlen_zero(tech_pvt->gateway_name)) {
			if ((p = (char *) switch_stristr("port=", url))) {
				p += 5;
				tech_pvt->transport = sofia_glue_str2transport(p);
			} else {
				if ((t_var = switch_channel_get_variable(channel, "sip_transport"))) {
					tech_pvt->transport = sofia_glue_str2transport(t_var);
				}
			}

			if (tech_pvt->transport == SOFIA_TRANSPORT_UNKNOWN) {
				tech_pvt->transport = SOFIA_TRANSPORT_UDP;
			}
		}

		if (sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
			tech_pvt->user_via = sofia_glue_create_external_via(session, tech_pvt->profile, tech_pvt->transport);
		}

		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_TLS) && sofia_glue_transport_has_tls(tech_pvt->transport)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "TLS not supported by profile\n");
			return SWITCH_STATUS_FALSE;
		}

		if (switch_strlen_zero(tech_pvt->invite_contact)) {
			const char * contact;
			if ((contact = switch_channel_get_variable(channel, "sip_contact_user"))) {
				char *ip_addr;
				char *ipv6;
				
				if (sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) { 
					ip_addr = (switch_check_network_list_ip(tech_pvt->remote_ip, tech_pvt->profile->local_network)) 
						? tech_pvt->profile->sipip : tech_pvt->profile->extsipip;
				} else {
					ip_addr = tech_pvt->profile->extsipip ? tech_pvt->profile->extsipip : tech_pvt->profile->sipip;
				}
				
				ipv6 = strchr(ip_addr, ':');

				if (sofia_glue_transport_has_tls(tech_pvt->transport)) {
					tech_pvt->invite_contact = switch_core_session_sprintf(session, "sip:%s@%s%s%s:%d", contact, 
																		   ipv6 ? "[" : "", ip_addr, ipv6 ? "]" : "", 
																		   tech_pvt->profile->tls_sip_port);
				} else {
					tech_pvt->invite_contact = switch_core_session_sprintf(session, "sip:%s@%s%s%s:%d", contact, 
																		   ipv6 ? "[" : "", ip_addr, ipv6 ? "]" : "", tech_pvt->profile->sip_port);
				}
			} else {
				if (sofia_glue_transport_has_tls(tech_pvt->transport)) {
					tech_pvt->invite_contact = tech_pvt->profile->tls_url;
				} else {
					if (sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) { 
						tech_pvt->invite_contact = tech_pvt->profile->public_url;
					} else {
						tech_pvt->invite_contact = tech_pvt->profile->url;
					}
				}
			}
		}

		url_str = sofia_overcome_sip_uri_weakness(session, url, tech_pvt->transport, SWITCH_TRUE, invite_params);
		invite_contact = sofia_overcome_sip_uri_weakness(session, tech_pvt->invite_contact, tech_pvt->transport, SWITCH_FALSE, invite_contact_params);
		from_str = sofia_overcome_sip_uri_weakness(session, use_from_str, 0, SWITCH_TRUE, invite_from_params);
		to_str = sofia_overcome_sip_uri_weakness(session, invite_to_uri ? invite_to_uri : tech_pvt->dest_to, 0, SWITCH_FALSE, invite_to_params);


		/*
		   Does the "genius" who wanted SIP to be "text-based" so it was "easier to read" even use it now,
		   or did he just suggest it to make our lives miserable?
		 */
		use_from_str = from_str;
		if (!from_display && !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
			from_str = switch_core_session_sprintf(session, "<%s>", use_from_str);
		} else {
			from_str = switch_core_session_sprintf(session, "\"%s\" <%s>", from_display ? from_display : 
												   tech_pvt->caller_profile->caller_id_name, use_from_str);
		}
		
		if (!(call_id = switch_channel_get_variable(channel, "sip_outgoing_call_id"))) {
			if (sofia_test_pflag(tech_pvt->profile, PFLAG_UUID_AS_CALLID)) {
				call_id = switch_core_session_get_uuid(session);
			}
		}
		
		tech_pvt->nh = nua_handle(tech_pvt->profile->nua, NULL,
								  NUTAG_URL(url_str),
								  TAG_IF(call_id, SIPTAG_CALL_ID_STR(call_id)),
								  SIPTAG_TO_STR(to_str), 
								  SIPTAG_FROM_STR(from_str),
								  SIPTAG_CONTACT_STR(invite_contact),
								  TAG_END());

		if (tech_pvt->dest && (strstr(tech_pvt->dest, ";fs_nat") || strstr(tech_pvt->dest, ";received") 
							   || ((val = switch_channel_get_variable(channel, "sip_sticky_contact")) && switch_true(val)))) {
			sofia_set_flag(tech_pvt, TFLAG_NAT);
			tech_pvt->record_route = switch_core_session_strdup(tech_pvt->session, url_str);
			route_uri = tech_pvt->record_route;
			session_timeout = SOFIA_NAT_SESSION_TIMEOUT;
			switch_channel_set_variable(channel, "sip_nat_detected", "true");
		}
		
		if ((val = switch_channel_get_variable(channel, "sip_cid_type"))) {
			cid_type = sofia_cid_name2type(val);
		}
		switch (cid_type) {
		case CID_TYPE_PID:
			if (switch_test_flag(caller_profile, SWITCH_CPF_SCREEN)) {
				tech_pvt->asserted_id = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>",
																	tech_pvt->caller_profile->caller_id_name,
																	tech_pvt->caller_profile->caller_id_number, 
																	rpid_domain);
			} else {
				tech_pvt->preferred_id = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>",
																	 tech_pvt->caller_profile->caller_id_name,
																	 tech_pvt->caller_profile->caller_id_number, 
																	 rpid_domain);
			}
			
			if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
				tech_pvt->privacy = "id";
			} else {
				tech_pvt->privacy = "none";
			}

			break;
		case CID_TYPE_RPID:
			{
				if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NAME)) {
					priv = "name";
					if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
						priv = "full";
					}
				} else if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
					priv = "full";
				}
				
				if (switch_test_flag(caller_profile, SWITCH_CPF_SCREEN)) {
					screen = "yes";
				}
				
				tech_pvt->rpid = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>;party=calling;screen=%s;privacy=%s",
															 tech_pvt->caller_profile->caller_id_name,
															 tech_pvt->caller_profile->caller_id_number, rpid_domain, screen, priv);
			}
			break;
		default:
			break;
		}


		switch_safe_free(d_url);

		if (!(sofia_private = malloc(sizeof(*sofia_private)))) {
			abort();
		}

		memset(sofia_private, 0, sizeof(*sofia_private));
		sofia_private->is_call++;

		tech_pvt->sofia_private = sofia_private;
		switch_copy_string(tech_pvt->sofia_private->uuid, switch_core_session_get_uuid(session), sizeof(tech_pvt->sofia_private->uuid));
		nua_handle_bind(tech_pvt->nh, tech_pvt->sofia_private);
	}

	if (tech_pvt->e_dest) {
		char *user = NULL, *host = NULL;
		char hash_key[256] = "";

		e_dest = strdup(tech_pvt->e_dest);
		switch_assert(e_dest != NULL);
		user = e_dest;

		if ((host = strchr(user, '@'))) {
			*host++ = '\0';
		}
		switch_snprintf(hash_key, sizeof(hash_key), "%s%s%s", user, host, cid_num);

		tech_pvt->chat_from = tech_pvt->from_str;
		tech_pvt->chat_to = tech_pvt->dest;
		if (tech_pvt->profile->pres_type) {
			tech_pvt->hash_key = switch_core_session_strdup(tech_pvt->session, hash_key);
			switch_mutex_lock(tech_pvt->profile->flag_mutex);
			switch_core_hash_insert(tech_pvt->profile->chat_hash, tech_pvt->hash_key, tech_pvt);
			switch_mutex_unlock(tech_pvt->profile->flag_mutex);
		}
		free(e_dest);
	}

	holdstr = sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD) ? "*" : "";

	if (!switch_channel_get_variable(channel, "sofia_profile_name")) {
		switch_channel_set_variable(channel, "sofia_profile_name", tech_pvt->profile->name);
	}

	extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_HEADER_PREFIX);

	session_timeout = tech_pvt->profile->session_timeout;
	if ((val = switch_channel_get_variable(channel, SOFIA_SESSION_TIMEOUT))) {
		int v_session_timeout = atoi(val);
		if (v_session_timeout >= 0) {
			session_timeout = v_session_timeout;
		}
	}

	if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			sofia_glue_tech_proxy_remote_addr(tech_pvt);
		}
		sofia_glue_tech_patch_sdp(tech_pvt);
	}

	if (!switch_strlen_zero(tech_pvt->dest)) {
		dst = sofia_glue_get_destination(tech_pvt->dest);

		if (dst->route_uri) {
			route_uri = sofia_overcome_sip_uri_weakness(tech_pvt->session, dst->route_uri, tech_pvt->transport, SWITCH_TRUE, NULL);
		}
	
		if (dst->route) {
			route = dst->route;
		}
	}

	if ((val = switch_channel_get_variable(channel, "sip_route_uri"))) {
		route_uri = switch_core_session_strdup(session, val);
		route = NULL;
	}
	
	if (route_uri) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s Setting proxy route to %s\n", route_uri, switch_channel_get_name(channel));
		tech_pvt->route_uri = switch_core_session_strdup(tech_pvt->session, route_uri);
	}

	nua_invite(tech_pvt->nh,
			   NUTAG_AUTOANSWER(0),
			   NUTAG_SESSION_TIMER(session_timeout),
			   TAG_IF(tech_pvt->redirected, NUTAG_URL(tech_pvt->redirected)),
			   TAG_IF(!switch_strlen_zero(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
			   TAG_IF(!switch_strlen_zero(tech_pvt->rpid), SIPTAG_REMOTE_PARTY_ID_STR(tech_pvt->rpid)),
			   TAG_IF(!switch_strlen_zero(tech_pvt->preferred_id), SIPTAG_P_PREFERRED_IDENTITY_STR(tech_pvt->preferred_id)),
			   TAG_IF(!switch_strlen_zero(tech_pvt->asserted_id), SIPTAG_P_ASSERTED_IDENTITY_STR(tech_pvt->asserted_id)),
			   TAG_IF(!switch_strlen_zero(tech_pvt->privacy), SIPTAG_PRIVACY_STR(tech_pvt->privacy)),
			   TAG_IF(!switch_strlen_zero(alert_info), SIPTAG_HEADER_STR(alert_info)),
			   TAG_IF(!switch_strlen_zero(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
			   SIPTAG_HEADER_STR("X-FS-Support: "FREESWITCH_SUPPORT),
			   TAG_IF(!switch_strlen_zero(max_forwards), SIPTAG_MAX_FORWARDS_STR(max_forwards)),
			   TAG_IF(!switch_strlen_zero(route_uri), NUTAG_PROXY(route_uri)),
			   TAG_IF(!switch_strlen_zero(route), SIPTAG_ROUTE_STR(route)),
			   SOATAG_ADDRESS(tech_pvt->adv_sdp_audio_ip),
			   SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
			   SOATAG_REUSE_REJECTED(1),
			   SOATAG_ORDERED_USER(1),
			   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE),
			   SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL), TAG_IF(rep, SIPTAG_REPLACES_STR(rep)), SOATAG_HOLD(holdstr), TAG_END());

	sofia_glue_free_destination(dst);
	switch_safe_free(extra_headers);
	tech_pvt->redirected = NULL;

	return SWITCH_STATUS_SUCCESS;
}

void sofia_glue_do_xfer_invite(switch_core_session_t *session)
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	const char *sipip, *format, *contact_url; 

	switch_assert(tech_pvt != NULL);
	switch_mutex_lock(tech_pvt->sofia_mutex);
	caller_profile = switch_channel_get_caller_profile(channel);

	if (sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) { 
		sipip = tech_pvt->profile->extsipip;
		contact_url = tech_pvt->profile->public_url;
	} else {
		sipip = tech_pvt->profile->extsipip ? tech_pvt->profile->extsipip : tech_pvt->profile->sipip;
		contact_url = tech_pvt->profile->url;
	}

	format = strchr(sipip, ':') ? "\"%s\" <sip:%s@[%s]>" : "\"%s\" <sip:%s@%s>";

	if ((tech_pvt->from_str = switch_core_session_sprintf(session, format,
															caller_profile->caller_id_name,
															caller_profile->caller_id_number, sipip))) {

		const char *rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);

		tech_pvt->nh2 = nua_handle(tech_pvt->profile->nua, NULL,
								   SIPTAG_TO_STR(tech_pvt->dest),
								   SIPTAG_FROM_STR(tech_pvt->from_str),
								   SIPTAG_CONTACT_STR(contact_url),
								   TAG_END());

		nua_handle_bind(tech_pvt->nh2, tech_pvt->sofia_private);

		nua_invite(tech_pvt->nh2,
				   SIPTAG_CONTACT_STR(contact_url),
				   TAG_IF(!switch_strlen_zero(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				   SOATAG_ADDRESS(tech_pvt->adv_sdp_audio_ip),
				   SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
				   SOATAG_REUSE_REJECTED(1),
				   SOATAG_ORDERED_USER(1),
				   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE), SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL), TAG_IF(rep, SIPTAG_REPLACES_STR(rep)), TAG_END());
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Memory Error!\n");
	}
	switch_mutex_unlock(tech_pvt->sofia_mutex);
}

void sofia_glue_tech_absorb_sdp(private_object_t *tech_pvt)
{
	const char *sdp_str;

	if ((sdp_str = switch_channel_get_variable(tech_pvt->channel, SWITCH_B_SDP_VARIABLE))) {
		sdp_parser_t *parser;
		sdp_session_t *sdp;
		sdp_media_t *m;
		sdp_connection_t *connection;

		if ((parser = sdp_parse(NULL, sdp_str, (int) strlen(sdp_str), 0))) {
			if ((sdp = sdp_session(parser))) {
				for (m = sdp->sdp_media; m; m = m->m_next) {
					if (m->m_type != sdp_media_audio || !m->m_port) {
						continue;
					}

					connection = sdp->sdp_connection;
					if (m->m_connections) {
						connection = m->m_connections;
					}

					if (connection) {
						tech_pvt->proxy_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, connection->c_address);
					}
					tech_pvt->proxy_sdp_audio_port = (switch_port_t) m->m_port;
					if (tech_pvt->proxy_sdp_audio_ip && tech_pvt->proxy_sdp_audio_port) {
						break;
					}
				}
			}
			sdp_parser_free(parser);
		}
		sofia_glue_tech_set_local_sdp(tech_pvt, sdp_str, SWITCH_TRUE);
	}
}


#define add_stat(_i, _s) \
	switch_snprintf(var_name, sizeof(var_name), "rtp_%s_%s", switch_str_nil(prefix), _s) ; \
	switch_snprintf(var_val, sizeof(var_val), "%" SWITCH_SIZE_T_FMT, _i); \
	switch_channel_set_variable(tech_pvt->channel, var_name, var_val)

static void set_stats(switch_rtp_t *rtp_session, private_object_t *tech_pvt, const char *prefix)
{
	switch_rtp_stats_t *stats = switch_rtp_get_stats(rtp_session, NULL);
	char var_name[256] = "", var_val[35] = "";

	if (stats) {

		add_stat(stats->inbound.raw_bytes, "in_raw_bytes");
		add_stat(stats->inbound.media_bytes, "in_media_bytes");
		add_stat(stats->inbound.packet_count, "in_packet_count");
		add_stat(stats->inbound.media_packet_count, "in_media_packet_count");
		add_stat(stats->inbound.skip_packet_count, "in_skip_packet_count");
		add_stat(stats->inbound.jb_packet_count, "in_jb_packet_count");
		add_stat(stats->inbound.dtmf_packet_count, "in_dtmf_packet_count");
		add_stat(stats->inbound.cng_packet_count, "in_cng_packet_count");
		add_stat(stats->inbound.cng_packet_count, "in_flush_packet_count");

		add_stat(stats->outbound.raw_bytes, "out_raw_bytes");
		add_stat(stats->outbound.media_bytes, "out_media_bytes");
		add_stat(stats->outbound.packet_count, "out_packet_count");
		add_stat(stats->outbound.media_packet_count, "out_media_packet_count");
		add_stat(stats->outbound.skip_packet_count, "out_skip_packet_count");
		add_stat(stats->outbound.dtmf_packet_count, "out_dtmf_packet_count");
		add_stat(stats->outbound.cng_packet_count, "out_cng_packet_count");
		
	}
}

void sofia_glue_set_rtp_stats(private_object_t *tech_pvt)
{
	if (tech_pvt->rtp_session) {
        set_stats(tech_pvt->rtp_session, tech_pvt, "audio");
	}

	if (tech_pvt->video_rtp_session) {
        set_stats(tech_pvt->video_rtp_session, tech_pvt, "video");
	}
}

void sofia_glue_deactivate_rtp(private_object_t *tech_pvt)
{
	int loops = 0;
	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		while (loops < 10 && (sofia_test_flag(tech_pvt, TFLAG_READING) || sofia_test_flag(tech_pvt, TFLAG_WRITING))) {
			switch_yield(10000);
			loops++;
		}
	}

	if (tech_pvt->rtp_session) {
		switch_rtp_destroy(&tech_pvt->rtp_session);
	} else if (tech_pvt->local_sdp_audio_port) {
		switch_rtp_release_port(tech_pvt->profile->rtpip, tech_pvt->local_sdp_audio_port);
	}

	if (tech_pvt->local_sdp_audio_port > 0 && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) { 
		switch_nat_del_mapping((switch_port_t)tech_pvt->local_sdp_audio_port, SWITCH_NAT_UDP);
	}
	
	if (tech_pvt->video_rtp_session) {
		switch_rtp_destroy(&tech_pvt->video_rtp_session);
	} else if (tech_pvt->local_sdp_video_port) {
		switch_rtp_release_port(tech_pvt->profile->rtpip, tech_pvt->local_sdp_video_port);
	}

		
	if (tech_pvt->local_sdp_video_port > 0 && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) { 
		switch_nat_del_mapping((switch_port_t)tech_pvt->local_sdp_video_port, SWITCH_NAT_UDP);
	}
}

switch_status_t sofia_glue_tech_set_video_codec(private_object_t *tech_pvt, int force)
{

	if (tech_pvt->video_read_codec.implementation && switch_core_codec_ready(&tech_pvt->video_read_codec)) {
		if (!force) {
			return SWITCH_STATUS_SUCCESS;
		}
		if (strcasecmp(tech_pvt->video_read_codec.implementation->iananame, tech_pvt->video_rm_encoding) ||
			tech_pvt->video_read_codec.implementation->samples_per_second != tech_pvt->video_rm_rate) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n",
							  tech_pvt->video_read_codec.implementation->iananame, tech_pvt->video_rm_encoding);
			switch_core_codec_destroy(&tech_pvt->video_read_codec);
			switch_core_codec_destroy(&tech_pvt->video_write_codec);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Already using %s\n", tech_pvt->video_read_codec.implementation->iananame);
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (!tech_pvt->video_rm_encoding) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec with no name?\n");
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init(&tech_pvt->video_read_codec,
							   tech_pvt->video_rm_encoding,
							   tech_pvt->video_rm_fmtp,
							   tech_pvt->video_rm_rate,
							   0,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&tech_pvt->video_write_codec,
								   tech_pvt->video_rm_encoding,
								   tech_pvt->video_rm_fmtp,
								   tech_pvt->video_rm_rate,
								   0,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
			return SWITCH_STATUS_FALSE;
		} else {
			int ms;
			tech_pvt->video_read_frame.rate = tech_pvt->video_rm_rate;
			ms = tech_pvt->video_write_codec.implementation->microseconds_per_packet / 1000;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set VIDEO Codec %s %s/%ld %d ms\n",
							  switch_channel_get_name(tech_pvt->channel), tech_pvt->video_rm_encoding, tech_pvt->video_rm_rate, tech_pvt->video_codec_ms);
			tech_pvt->video_read_frame.codec = &tech_pvt->video_read_codec;

			tech_pvt->video_fmtp_out = switch_core_session_strdup(tech_pvt->session, tech_pvt->video_write_codec.fmtp_out);

			tech_pvt->video_write_codec.agreed_pt = tech_pvt->video_agreed_pt;
			tech_pvt->video_read_codec.agreed_pt = tech_pvt->video_agreed_pt;
			switch_core_session_set_video_read_codec(tech_pvt->session, &tech_pvt->video_read_codec);
			switch_core_session_set_video_write_codec(tech_pvt->session, &tech_pvt->video_write_codec);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t sofia_glue_tech_set_codec(private_object_t *tech_pvt, int force)
{
	int ms;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int resetting = 0;
	
	if (!tech_pvt->iananame) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "No audio codec available\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (tech_pvt->read_codec.implementation && switch_core_codec_ready(&tech_pvt->read_codec)) {
		if (!force) {
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
		if (strcasecmp(tech_pvt->read_codec.implementation->iananame, tech_pvt->iananame) ||
			tech_pvt->read_codec.implementation->samples_per_second != tech_pvt->rm_rate) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n",
							  tech_pvt->read_codec.implementation->iananame, tech_pvt->rm_encoding);
			switch_core_session_lock_codec_write(tech_pvt->session);
			switch_core_session_lock_codec_read(tech_pvt->session);
			resetting = 1;
			switch_core_codec_destroy(&tech_pvt->read_codec);
			switch_core_codec_destroy(&tech_pvt->write_codec);
			switch_core_session_reset(tech_pvt->session, SWITCH_TRUE, SWITCH_TRUE);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Already using %s\n", tech_pvt->read_codec.implementation->iananame);
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
	}
	
	if (switch_core_codec_init(&tech_pvt->read_codec,
							   tech_pvt->iananame,
							   tech_pvt->rm_fmtp,
							   tech_pvt->rm_rate,
							   tech_pvt->codec_ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | tech_pvt->profile->codec_flags,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (switch_core_codec_init(&tech_pvt->write_codec,
							   tech_pvt->iananame,
							   tech_pvt->rm_fmtp,
							   tech_pvt->rm_rate,
							   tech_pvt->codec_ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | tech_pvt->profile->codec_flags,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_assert(tech_pvt->read_codec.implementation);
	switch_assert(tech_pvt->write_codec.implementation);

	tech_pvt->read_impl = *tech_pvt->read_codec.implementation;
	tech_pvt->write_impl = *tech_pvt->write_codec.implementation;

	switch_core_session_set_read_impl(tech_pvt->session, tech_pvt->read_codec.implementation);
	switch_core_session_set_write_impl(tech_pvt->session, tech_pvt->write_codec.implementation);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_assert(tech_pvt->read_codec.implementation);
		switch_rtp_set_interval(tech_pvt->rtp_session,
								   tech_pvt->write_codec.implementation->microseconds_per_packet,
								   tech_pvt->read_impl.samples_per_packet);
	}

	tech_pvt->read_frame.rate = tech_pvt->rm_rate;
	ms = tech_pvt->write_codec.implementation->microseconds_per_packet / 1000;

	if (!switch_core_codec_ready(&tech_pvt->read_codec)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Codec %s %s/%ld %d ms %d samples\n",
					  switch_channel_get_name(tech_pvt->channel), tech_pvt->iananame, tech_pvt->rm_rate, tech_pvt->codec_ms,
					  tech_pvt->read_impl.samples_per_packet);
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;

	tech_pvt->write_codec.agreed_pt = tech_pvt->agreed_pt;
	tech_pvt->read_codec.agreed_pt = tech_pvt->agreed_pt;

	if (force != 2) {
		switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
		switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
	}

	tech_pvt->fmtp_out = switch_core_session_strdup(tech_pvt->session, tech_pvt->write_codec.fmtp_out);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_rtp_set_default_payload(tech_pvt->rtp_session, tech_pvt->pt);
	}

 end:
	if (resetting) {
		switch_core_session_unlock_codec_write(tech_pvt->session);
		switch_core_session_unlock_codec_read(tech_pvt->session);
	}

	return status;
}


switch_status_t sofia_glue_build_crypto(private_object_t *tech_pvt, int index, switch_rtp_crypto_key_type_t type, switch_rtp_crypto_direction_t direction)
{
	unsigned char b64_key[512] = "";
	const char *type_str;
	unsigned char *key;
	const char *val;

	char *p;

	if (type == AES_CM_128_HMAC_SHA1_80) {
		type_str = SWITCH_RTP_CRYPTO_KEY_80;
	} else {
		type_str = SWITCH_RTP_CRYPTO_KEY_32;
	}

	if (direction == SWITCH_RTP_CRYPTO_SEND) {
		key = tech_pvt->local_raw_key;
	} else {
		key = tech_pvt->remote_raw_key;

	}

	switch_rtp_get_random(key, SWITCH_RTP_KEY_LEN);
	switch_b64_encode(key, SWITCH_RTP_KEY_LEN, b64_key, sizeof(b64_key));
	p = strrchr((char *) b64_key, '=');

	while (p && *p && *p == '=') {
		*p-- = '\0';
	}

	tech_pvt->local_crypto_key = switch_core_session_sprintf(tech_pvt->session, "%d %s inline:%s", index, type_str, b64_key);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Local Key [%s]\n", tech_pvt->local_crypto_key);

	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_SRTP_AUTH) &&
		!((val = switch_channel_get_variable(tech_pvt->channel, "NDLB_support_asterisk_missing_srtp_auth")) && switch_true(val))) {
		tech_pvt->crypto_type = type;
	} else {
		tech_pvt->crypto_type = AES_CM_128_NULL_AUTH;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t sofia_glue_add_crypto(private_object_t *tech_pvt, const char *key_str, switch_rtp_crypto_direction_t direction)
{
	unsigned char key[SWITCH_RTP_MAX_CRYPTO_LEN];
	int index;
	switch_rtp_crypto_key_type_t type;
	char *p;


	if (!switch_rtp_ready(tech_pvt->rtp_session)) {
		goto bad;
	}

	index = atoi(key_str);

	p = strchr(key_str, ' ');

	if (p && *p && *(p + 1)) {
		p++;
		if (!strncasecmp(p, SWITCH_RTP_CRYPTO_KEY_32, strlen(SWITCH_RTP_CRYPTO_KEY_32))) {
			type = AES_CM_128_HMAC_SHA1_32;
		} else if (!strncasecmp(p, SWITCH_RTP_CRYPTO_KEY_80, strlen(SWITCH_RTP_CRYPTO_KEY_80))) {
			type = AES_CM_128_HMAC_SHA1_80;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Parse Error near [%s]\n", p);
			goto bad;
		}

		p = strchr(p, ' ');
		if (p && *p && *(p + 1)) {
			p++;
			if (strncasecmp(p, "inline:", 7)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Parse Error near [%s]\n", p);
				goto bad;
			}

			p += 7;
			switch_b64_decode(p, (char *) key, sizeof(key));

			if (direction == SWITCH_RTP_CRYPTO_SEND) {
				tech_pvt->crypto_send_type = type;
				memcpy(tech_pvt->local_raw_key, key, SWITCH_RTP_KEY_LEN);
			} else {
				tech_pvt->crypto_recv_type = type;
				memcpy(tech_pvt->remote_raw_key, key, SWITCH_RTP_KEY_LEN);
			}
			return SWITCH_STATUS_SUCCESS;
		}

	}

  bad:

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Error!\n");
	return SWITCH_STATUS_FALSE;

}


switch_status_t sofia_glue_activate_rtp(private_object_t *tech_pvt, switch_rtp_flag_t myflags)
{
	int bw, ms;
	const char *err = NULL;
	const char *val = NULL;
	switch_rtp_flag_t flags;
	switch_status_t status;
	char tmp[50];
	uint32_t rtp_timeout_sec = tech_pvt->profile->rtp_timeout_sec;
	uint32_t rtp_hold_timeout_sec = tech_pvt->profile->rtp_hold_timeout_sec;
	char *timer_name = NULL;
	const char *var;

	switch_assert(tech_pvt != NULL);

	if (switch_channel_down(tech_pvt->channel) || sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(tech_pvt->sofia_mutex);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_rtp_reset_media_timer(tech_pvt->rtp_session);
	}

	if ((var = switch_channel_get_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_VARIABLE)) && switch_true(var)) {
		sofia_set_flag_locked(tech_pvt, TFLAG_SECURE);
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE)) {
		status = SWITCH_STATUS_SUCCESS;
		goto end;
	}

	if (switch_rtp_ready(tech_pvt->rtp_session) && !sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
		status = SWITCH_STATUS_SUCCESS;
		goto end;
	}

	if ((status = sofia_glue_tech_set_codec(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
		goto end;
	}

	bw = tech_pvt->read_impl.bits_per_second;
	ms = tech_pvt->read_impl.microseconds_per_packet;

	if (myflags) {
		flags = myflags;
	} else if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
		flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);
    } else {
		flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_DATAWAIT);
	}

	if ((val = switch_channel_get_variable(tech_pvt->channel, "dtmf_type"))) {
		if (!strcasecmp(val, "rfc2833")) {
			tech_pvt->dtmf_type = DTMF_2833;
		} else if (!strcasecmp(val, "info")) {
			tech_pvt->dtmf_type = DTMF_INFO;
		} else {
			tech_pvt->dtmf_type = tech_pvt->profile->dtmf_type;
		}
	}

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_PASS_RFC2833)
		|| ((val = switch_channel_get_variable(tech_pvt->channel, "pass_rfc2833")) && switch_true(val))) {
		sofia_set_flag(tech_pvt, TFLAG_PASS_RFC2833);
	}


	if (sofia_test_pflag(tech_pvt->profile, PFLAG_AUTOFLUSH)
		|| ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_autoflush")) && switch_true(val))) {
		flags |= SWITCH_RTP_FLAG_AUTOFLUSH;
	}

	if (!(sofia_test_pflag(tech_pvt->profile, PFLAG_REWRITE_TIMESTAMPS) ||
		  ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_rewrite_timestamps")) && !switch_true(val)))) {
		flags |= SWITCH_RTP_FLAG_RAW_WRITE;
	}

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG)) {
		tech_pvt->cng_pt = 0;
	} else if (tech_pvt->cng_pt) {
		flags |= SWITCH_RTP_FLAG_AUTO_CNG;
	}

	if (tech_pvt->rtp_session && sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
		//const char *ip = switch_channel_get_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
		//const char *port = switch_channel_get_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);
		char *remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);

		if (remote_host && remote_port && !strcmp(remote_host, tech_pvt->remote_sdp_audio_ip) && remote_port == tech_pvt->remote_sdp_audio_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Audio params are unchanged for %s.\n", switch_channel_get_name(tech_pvt->channel));
			goto video;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Audio params changed for %s from %s:%d to %s:%d\n", 
							  switch_channel_get_name(tech_pvt->channel),
							  remote_host, remote_port, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
		}
	}

	if (!switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "AUDIO RTP [%s] %s port %d -> %s port %d codec: %u ms: %d\n",
						  switch_channel_get_name(tech_pvt->channel),
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port,
						  tech_pvt->remote_sdp_audio_ip,
						  tech_pvt->remote_sdp_audio_port, tech_pvt->agreed_pt, tech_pvt->read_impl.microseconds_per_packet / 1000);
	}

	switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->local_sdp_audio_port);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, tmp);

	if (tech_pvt->rtp_session && sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
		sofia_clear_flag_locked(tech_pvt, TFLAG_REINVITE);
		
		if (switch_rtp_set_remote_address(tech_pvt->rtp_session, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port, SWITCH_TRUE, &err) !=
			SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "AUDIO RTP CHANGING DEST TO: [%s:%d]\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				/* Reactivate the NAT buster flag. */
				switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
		}
		goto video;
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
		if ((status = sofia_glue_tech_proxy_remote_addr(tech_pvt)) != SWITCH_STATUS_SUCCESS) {
			goto end;
		}

		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
			!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
			flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_PROXY_MEDIA | SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);
		} else {
			flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_PROXY_MEDIA | SWITCH_RTP_FLAG_DATAWAIT);
		}
		timer_name = NULL;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG,
						  "PROXY AUDIO RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
						  switch_channel_get_name(tech_pvt->channel),
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port,
						  tech_pvt->remote_sdp_audio_ip,
						  tech_pvt->remote_sdp_audio_port, tech_pvt->agreed_pt, tech_pvt->read_impl.microseconds_per_packet / 1000);

	} else {
		timer_name = tech_pvt->profile->timer_name;

		if ((var = switch_channel_get_variable(tech_pvt->channel, "rtp_timer_name"))) {
			timer_name = (char *) var;
		}
	}

	if (switch_channel_up(tech_pvt->channel) && !sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		tech_pvt->rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
											   tech_pvt->local_sdp_audio_port,
											   tech_pvt->remote_sdp_audio_ip,
											   tech_pvt->remote_sdp_audio_port,
											   tech_pvt->agreed_pt,
											   tech_pvt->read_impl.samples_per_packet,
											   tech_pvt->codec_ms * 1000,
											   (switch_rtp_flag_t) flags, timer_name, &err,
											   switch_core_session_get_pool(tech_pvt->session));
	}

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		uint8_t vad_in = sofia_test_flag(tech_pvt, TFLAG_VAD_IN) ? 1 : 0;
		uint8_t vad_out = sofia_test_flag(tech_pvt, TFLAG_VAD_OUT) ? 1 : 0;
		uint8_t inb = sofia_test_flag(tech_pvt, TFLAG_OUTBOUND) ? 0 : 1;
		uint32_t stun_ping = 0;

		switch_channel_set_flag(tech_pvt->channel, CF_FS_RTP);
		
		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_enable_vad_in")) && switch_true(val)) {
			vad_in = 1;
		}
		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_enable_vad_out")) && switch_true(val)) {
			vad_out = 1;
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_disable_vad_in")) && switch_true(val)) {
			vad_in = 0;
		}
		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_disable_vad_out")) && switch_true(val)) {
			vad_out = 0;
		}

		if ((tech_pvt->stun_flags & STUN_FLAG_SET) && (val = switch_channel_get_variable(tech_pvt->channel, "rtp_stun_ping"))) {
			int ival = atoi(val);
			
			if (ival <= 0) {
				if (switch_true(val)) {
					ival = 6;
				}
			}

			stun_ping = (ival * tech_pvt->read_impl.samples_per_second) / tech_pvt->read_impl.samples_per_packet;
		}

		tech_pvt->ssrc = switch_rtp_get_ssrc(tech_pvt->rtp_session);
		sofia_set_flag(tech_pvt, TFLAG_RTP);
		sofia_set_flag(tech_pvt, TFLAG_IO);
		
		switch_rtp_intentional_bugs(tech_pvt->rtp_session, tech_pvt->rtp_bugs);
		
		if ((vad_in && inb) || (vad_out && !inb)) {
			switch_rtp_enable_vad(tech_pvt->rtp_session, tech_pvt->session, &tech_pvt->read_codec, SWITCH_VAD_FLAG_TALKING);
			sofia_set_flag(tech_pvt, TFLAG_VAD);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "AUDIO RTP Engage VAD for %s ( %s %s )\n",
							  switch_channel_get_name(switch_core_session_get_channel(tech_pvt->session)), vad_in ? "in" : "", vad_out ? "out" : "");
		}

		if (stun_ping) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Setting stun ping to %s:%d\n", tech_pvt->stun_ip, stun_ping);
			switch_rtp_activate_stun_ping(tech_pvt->rtp_session, tech_pvt->stun_ip, tech_pvt->stun_port, 
										  stun_ping, (tech_pvt->stun_flags & STUN_FLAG_FUNNY) ? 1 : 0);
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "jitterbuffer_msec"))) {
			int len = atoi(val);

			if (len < 100 || len > 1000) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Invalid Jitterbuffer spec [%d] must be between 100 and 1000\n", len);
			} else {
				int qlen;

				qlen = len / (tech_pvt->read_impl.microseconds_per_packet / 1000);

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Setting Jitterbuffer to %dms (%d frames)\n", len, qlen);
				switch_rtp_activate_jitter_buffer(tech_pvt->rtp_session, qlen);
			}
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_timeout_sec"))) {
			int v = atoi(val);
			if (v >= 0) {
				rtp_timeout_sec = v;
			}
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_hold_timeout_sec"))) {
			int v = atoi(val);
			if (v >= 0) {
				rtp_hold_timeout_sec = v;
			}
		}

		if (rtp_timeout_sec) {
			tech_pvt->max_missed_packets = (tech_pvt->read_impl.samples_per_second * rtp_timeout_sec) /
				tech_pvt->read_impl.samples_per_packet;

			switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_packets);
			if (!rtp_hold_timeout_sec) {
				rtp_hold_timeout_sec = rtp_timeout_sec * 10;
			}
		}

		if (rtp_hold_timeout_sec) {
			tech_pvt->max_missed_hold_packets = (tech_pvt->read_impl.samples_per_second * rtp_hold_timeout_sec) /
				tech_pvt->read_impl.samples_per_packet;
		}

		if (tech_pvt->te) {
			switch_rtp_set_telephony_event(tech_pvt->rtp_session, tech_pvt->te);
		}

		if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) ||
			((val = switch_channel_get_variable(tech_pvt->channel, "supress_cng")) && switch_true(val)) ||
			((val = switch_channel_get_variable(tech_pvt->channel, "suppress_cng")) && switch_true(val))) {
			tech_pvt->cng_pt = 0;
		}

		if (tech_pvt->cng_pt && !sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", tech_pvt->cng_pt);
			switch_rtp_set_cng_pt(tech_pvt->rtp_session, tech_pvt->cng_pt);
		}

		if (tech_pvt->remote_crypto_key && sofia_test_flag(tech_pvt, TFLAG_SECURE)) {
			sofia_glue_add_crypto(tech_pvt, tech_pvt->remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
			switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_SEND, 1, tech_pvt->crypto_type, tech_pvt->local_raw_key,
									  SWITCH_RTP_KEY_LEN);
			switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_RECV, tech_pvt->crypto_tag, tech_pvt->crypto_type, tech_pvt->remote_raw_key,
									  SWITCH_RTP_KEY_LEN);
			switch_channel_set_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_CONFIRMED_VARIABLE, "true");
		}

	  video:

		sofia_glue_check_video_codecs(tech_pvt);

		if (sofia_test_flag(tech_pvt, TFLAG_VIDEO) && tech_pvt->video_rm_encoding && tech_pvt->remote_sdp_video_port) {
			if (!tech_pvt->local_sdp_video_port) {
				sofia_glue_tech_choose_video_port(tech_pvt, 1);
			}

			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_USE_TIMER | SWITCH_RTP_FLAG_AUTOADJ |
		 									 SWITCH_RTP_FLAG_DATAWAIT | SWITCH_RTP_FLAG_NOBLOCK | SWITCH_RTP_FLAG_RAW_WRITE);
			} else {
				flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_USE_TIMER | SWITCH_RTP_FLAG_DATAWAIT | SWITCH_RTP_FLAG_NOBLOCK | SWITCH_RTP_FLAG_RAW_WRITE);
			}

			if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
				flags |= SWITCH_RTP_FLAG_PROXY_MEDIA;
			}
			sofia_glue_tech_set_video_codec(tech_pvt, 0);

			/* set video timer to 10ms so it can co-exist with audio */
			tech_pvt->video_rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
														 tech_pvt->local_sdp_video_port,
														 tech_pvt->remote_sdp_video_ip,
														 tech_pvt->remote_sdp_video_port,
														 tech_pvt->video_agreed_pt,
														 1, 10000, (switch_rtp_flag_t) flags, NULL, &err, switch_core_session_get_pool(tech_pvt->session));

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%sVIDEO RTP [%s] %s:%d->%s:%d codec: %u ms: %d [%s]\n",
							  switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) ? "PROXY " : "",
							  switch_channel_get_name(tech_pvt->channel),
							  tech_pvt->local_sdp_audio_ip,
							  tech_pvt->local_sdp_video_port,
							  tech_pvt->remote_sdp_video_ip,
							  tech_pvt->remote_sdp_video_port, tech_pvt->video_agreed_pt,
							  0, switch_rtp_ready(tech_pvt->video_rtp_session) ? "SUCCESS" : err);

			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				switch_channel_set_flag(tech_pvt->channel, CF_VIDEO);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				goto end;
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		sofia_clear_flag_locked(tech_pvt, TFLAG_IO);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	sofia_set_flag(tech_pvt, TFLAG_IO);
	status = SWITCH_STATUS_SUCCESS;

  end:

	switch_mutex_unlock(tech_pvt->sofia_mutex);

	return status;

}

void sofia_glue_set_r_sdp_codec_string(switch_channel_t *channel,const char *codec_string, sdp_session_t *sdp)
{
	char buf[1024] = {0};
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int ptime = 0, dptime = 0;
	sdp_connection_t *connection;
	sdp_rtpmap_t *map;
	short int match = 0;
	int i;
	int already_did[128] = { 0 };
	int num_codecs = 0;
	char *codec_order[SWITCH_MAX_CODECS];
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS] = { 0 };

	if (!switch_strlen_zero(codec_string)) {
		char *tmp_codec_string;
		if ((tmp_codec_string = strdup(codec_string))) {
			num_codecs = switch_separate_string(tmp_codec_string, ',', codec_order, SWITCH_MAX_CODECS);
			num_codecs = switch_loadable_module_get_codecs_sorted(codecs, SWITCH_MAX_CODECS, codec_order, num_codecs);
			switch_safe_free(tmp_codec_string);
		}
	} else {
		num_codecs = switch_loadable_module_get_codecs(codecs, SWITCH_MAX_CODECS);
	}

	if (!channel || !num_codecs) {
		return;
	}

	for (attr = sdp->sdp_attributes; attr; attr = attr->a_next) {
		if (switch_strlen_zero(attr->a_name)) {
			continue;
		}

		if (!strcasecmp(attr->a_name, "ptime")) {
			dptime = atoi(attr->a_value);
			break;
		}
	}

	for (m = sdp->sdp_media; m; m = m->m_next) {
		ptime = dptime;
		if ( m->m_type == sdp_media_image && m->m_port) {
			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",t38");
		} else if (m->m_type == sdp_media_audio && m->m_port) {
			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (switch_strlen_zero(attr->a_name)) {
					continue;
				}
				if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
					ptime = atoi(attr->a_value);
					break;
				}
			}
			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				break;
			}

			for (i = 0; i < num_codecs; i++) {
				const switch_codec_implementation_t *imp = codecs[i];
				if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO || imp->ianacode > 127 || already_did[imp->ianacode]) {
					continue;
				}
				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if ( map->rm_pt > 127 ||  already_did[map->rm_pt]) {
						continue;
					}

					if (map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						if (map->rm_encoding) {
							match = strcasecmp(map->rm_encoding, imp->iananame) ? 0 : 1;
						} else {
							match = 0;
						}
					}

					if (match) {
						if (ptime > 0) {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s@%uh@%di", imp->iananame, (unsigned int) map->rm_rate, ptime);
						} else {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s@%uh", imp->iananame, (unsigned int) map->rm_rate);
						}
						already_did[imp->ianacode] = 1;
						break;
					}
				}
			}
		} else if (m->m_type == sdp_media_video && m->m_port) {
			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				break;
			}
			for (i = 0; i < num_codecs; i++) {
				const switch_codec_implementation_t *imp = codecs[i];
				if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO || imp->ianacode > 127 || already_did[imp->ianacode]) {
					continue;
				}
				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if ( map->rm_pt > 127 || already_did[map->rm_pt]) {
						continue;
					}

					if (map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						if(map->rm_encoding) {
							match = strcasecmp(map->rm_encoding, imp->iananame) ? 0 : 1;
						} else {
							match = 0;
						}
					}

					if (match) {
						if(ptime > 0) {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s@%uh@%di", imp->iananame, (unsigned int) map->rm_rate, ptime);
						} else {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s@%uh", imp->iananame, (unsigned int) map->rm_rate);
						}
						already_did[imp->ianacode] = 1;
						break;
					}
				}
			}
		}
	}
	if (buf[0] == ',') {
		switch_channel_set_variable(channel, "ep_codec_string", buf + 1);
	}
}

switch_status_t sofia_glue_tech_media(private_object_t *tech_pvt, const char *r_sdp)
{
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	uint8_t match = 0;

	switch_assert(tech_pvt != NULL);
	switch_assert(r_sdp != NULL);

	if (switch_strlen_zero(r_sdp)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		
		if (tech_pvt->num_codecs) {
			if ((sdp = sdp_session(parser))) {
				match = sofia_glue_negotiate_sdp(tech_pvt->session, sdp);
			}
		}
		
		sdp_parser_free(parser);
	}

	if (match) {
		if (sofia_glue_tech_choose_port(tech_pvt, 0) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		if (sofia_glue_activate_rtp(tech_pvt, 0) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "EARLY MEDIA");
		sofia_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
		switch_channel_mark_pre_answered(tech_pvt->channel);
		return SWITCH_STATUS_SUCCESS;
	}


	return SWITCH_STATUS_FALSE;
}

void sofia_glue_toggle_hold(private_object_t *tech_pvt, int sendonly)
{
	if (sendonly && switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED)) {
		if (!sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
			const char *stream;

			sofia_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			switch_channel_presence(tech_pvt->channel, "unknown", "hold", NULL);

			if (tech_pvt->max_missed_hold_packets) {
				switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_hold_packets);
			}

			if (!(stream = switch_channel_get_variable(tech_pvt->channel, SWITCH_HOLD_MUSIC_VARIABLE))) {
				stream = tech_pvt->profile->hold_music;
			}

			if (stream && strcasecmp(stream, "silence")) {
				if (!strcasecmp(stream, "indicate_hold")) {
					switch_channel_set_flag(tech_pvt->channel, CF_SUSPEND);
					switch_channel_set_flag(tech_pvt->channel, CF_HOLD);
					switch_ivr_hold_uuid(switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE), NULL, 0);
				} else {
					switch_ivr_broadcast(switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE), stream, SMF_ECHO_ALEG | SMF_LOOP);
					switch_yield(250000);
				}
			}
		}
	} else {
		if (sofia_test_flag(tech_pvt, TFLAG_HOLD_LOCK)) {
			sofia_set_flag(tech_pvt, TFLAG_SIP_HOLD);
		}

		sofia_clear_flag_locked(tech_pvt, TFLAG_HOLD_LOCK);
		
		if (sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
			const char *uuid;
			switch_core_session_t *b_session;

			switch_yield(250000);

			if (tech_pvt->max_missed_packets) {
				switch_rtp_reset_media_timer(tech_pvt->rtp_session);
				switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_packets);
			}

			if ((uuid = switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE)) && (b_session = switch_core_session_locate(uuid))) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);

				if (switch_channel_test_flag(tech_pvt->channel, CF_HOLD)) {
					switch_ivr_unhold(b_session);
					switch_channel_clear_flag(tech_pvt->channel, CF_SUSPEND);
					switch_channel_clear_flag(tech_pvt->channel, CF_HOLD);
				} else {
					switch_channel_stop_broadcast(b_channel);
					switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
				}
				switch_core_session_rwunlock(b_session);
			}

			sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			switch_channel_presence(tech_pvt->channel, "unknown", "unhold", NULL);
		}
	}
}

uint8_t sofia_glue_negotiate_sdp(switch_core_session_t *session, sdp_session_t *sdp)
{
	uint8_t match = 0;
	switch_payload_t te = 0, cng_pt = 0;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int first = 0, last = 0;
	int ptime = 0, dptime = 0, maxptime = 0, dmaxptime = 0;
	int sendonly = 0;
	int greedy = 0, x = 0, skip = 0, mine = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *val;
	const char *crypto = NULL;
	int got_crypto = 0, got_audio = 0, got_avp = 0, got_savp = 0, got_udptl = 0;
	int scrooge = 0;

	switch_assert(tech_pvt != NULL);

	greedy = !!sofia_test_pflag(tech_pvt->profile, PFLAG_GREEDY);
	scrooge = !!sofia_test_pflag(tech_pvt->profile, PFLAG_SCROOGE);
	
	if (!greedy || !scrooge) {
		if ((val = switch_channel_get_variable(channel, "sip_codec_negotiation"))) {
			if (!strcasecmp(val, "greedy")) {
				greedy = 1;
			} else if (!strcasecmp(val, "scrooge")) {
				scrooge = 1;
				greedy = 1;
			}
		}
	}

	if ((tech_pvt->origin = switch_core_session_strdup(session, (char *) sdp->sdp_origin->o_username))) {

		if (tech_pvt->profile->auto_rtp_bugs & RTP_BUG_CISCO_SKIP_MARK_BIT_2833) {

			if (strstr(tech_pvt->origin, "CiscoSystemsSIP-GW-UserAgent")) {
				tech_pvt->rtp_bugs |= RTP_BUG_CISCO_SKIP_MARK_BIT_2833;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activate Buggy RFC2833 Mode!\n");
			}
		}
		
		if (tech_pvt->profile->auto_rtp_bugs & RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833) {
			if (strstr(tech_pvt->origin, "Sonus_UAC")) {
				tech_pvt->rtp_bugs |= RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, 
								  "Hello,\nI see you have a Sonus!\n"
								  "FYI, Sonus cannot follow the RFC on the proper way to send DTMF.\n"
								  "Sadly, my creator had to spend several hours figuring this out so I thought you'd like to know that!\n"
								  "Don't worry, DTMF will work but you may want to ask them to fix it......\n"
								  );
			}
		}
	}

	if ((m = sdp->sdp_media) && (m->m_mode == sdp_sendonly || m->m_mode == sdp_inactive)) {
		sendonly = 2;			/* global sendonly always wins */
	}

	for (attr = sdp->sdp_attributes; attr; attr = attr->a_next) {
		if (switch_strlen_zero(attr->a_name)) {
			continue;
		}

		if (!strcasecmp(attr->a_name, "sendonly") || !strcasecmp(attr->a_name, "inactive")) {
			sendonly = 1;
		} else if (sendonly < 2 && !strcasecmp(attr->a_name, "sendrecv")) {
			sendonly = 0;
		} else if (!strcasecmp(attr->a_name, "ptime")) {
			dptime = atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "maxptime")) {
			dmaxptime = atoi(attr->a_value);
		}
	}

	if (!tech_pvt->hold_laps) {
		tech_pvt->hold_laps++;
		sofia_glue_toggle_hold(tech_pvt, sendonly);
	}

	for (m = sdp->sdp_media; m; m = m->m_next) {
		sdp_connection_t *connection;

		ptime = dptime;
		maxptime = dmaxptime;

		if (m->m_proto == sdp_proto_srtp) {
			got_savp++;
		} else if (m->m_proto == sdp_proto_rtp) {
			got_avp++;
		} else if (m->m_proto == sdp_proto_udptl) {
			got_udptl++;
		}

		if (got_udptl && m->m_type == sdp_media_image && m->m_port) {
			switch_t38_options_t *t38_options = switch_core_session_alloc(tech_pvt->session, sizeof(switch_t38_options_t));
			
			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "T38MaxBitRate") && attr->a_value) { 
					t38_options->T38MaxBitRate = (uint32_t) atoi(attr->a_value);
				} else  if (!strcasecmp(attr->a_name, "T38FaxFillBitRemoval")) {
					t38_options->T38FaxFillBitRemoval = SWITCH_TRUE;
				} else  if (!strcasecmp(attr->a_name, "T38FaxTranscodingMMR")) {
					t38_options->T38FaxTranscodingMMR = SWITCH_TRUE;
				} else  if (!strcasecmp(attr->a_name, "T38FaxTranscodingJBIG")) {
					t38_options->T38FaxTranscodingJBIG = SWITCH_TRUE;
				} else  if (!strcasecmp(attr->a_name, "T38FaxRateManagement") && attr->a_value) {
					t38_options->T38FaxRateManagement = switch_core_session_strdup(tech_pvt->session, attr->a_value);
				} else  if (!strcasecmp(attr->a_name, "T38FaxMaxBuffer") && attr->a_value) {
					t38_options->T38FaxMaxBuffer = (uint32_t) atoi(attr->a_value);
				} else  if (!strcasecmp(attr->a_name, "T38FaxMaxDatagram") && attr->a_value) {
					t38_options->T38FaxMaxDatagram = (uint32_t) atoi(attr->a_value);
				} else  if (!strcasecmp(attr->a_name, "T38FaxUdpEC") && attr->a_value) {
					t38_options->T38FaxUdpEC = switch_core_session_strdup(tech_pvt->session, attr->a_value);
				} else  if (!strcasecmp(attr->a_name, "T38VendorInfo") && attr->a_value) {
					t38_options->T38VendorInfo = switch_core_session_strdup(tech_pvt->session, attr->a_value);
				}
			}

			switch_channel_set_variable(tech_pvt->channel, "has_t38", "true");
			switch_channel_set_private(tech_pvt->channel, "t38_options", t38_options);

			//switch_channel_set_flag(tech_pvt->channel, CF_PROXY_MEDIA);
			//switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA);

		} else if (m->m_type == sdp_media_audio && m->m_port && !got_audio) {
			sdp_rtpmap_t *map;
			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
					ptime = atoi(attr->a_value);
				} else if (!strcasecmp(attr->a_name, "maxptime") && attr->a_value) {
					maxptime = atoi(attr->a_value);
				} else if (!got_crypto && !strcasecmp(attr->a_name, "crypto") && !switch_strlen_zero(attr->a_value)) {
					int crypto_tag;

					if (m->m_proto != sdp_proto_srtp) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
						match = 0;
						goto done;
					}

					crypto = attr->a_value;
					crypto_tag = atoi(crypto);

					if (tech_pvt->remote_crypto_key && switch_rtp_ready(tech_pvt->rtp_session)) {
						if (crypto_tag && crypto_tag == tech_pvt->crypto_tag) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Existing key is still valid.\n");
						} else {
							const char *a = switch_stristr("AES", tech_pvt->remote_crypto_key);
							const char *b = switch_stristr("AES", crypto);

							/* Change our key every time we can */
							if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_32, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_32);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
								switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_SEND, atoi(crypto), tech_pvt->crypto_type,
														  tech_pvt->local_raw_key, SWITCH_RTP_KEY_LEN);
							} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_80, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_80);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
								switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_SEND, atoi(crypto), tech_pvt->crypto_type,
														  tech_pvt->local_raw_key, SWITCH_RTP_KEY_LEN);
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto Setup Failed!.\n");
							}
														
							if (a && b && !strncasecmp(a, b, 23)) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Change Remote key to [%s]\n", crypto);
								tech_pvt->remote_crypto_key = switch_core_session_strdup(tech_pvt->session, crypto);
								tech_pvt->crypto_tag = crypto_tag;
								
								if (switch_rtp_ready(tech_pvt->rtp_session) && sofia_test_flag(tech_pvt, TFLAG_SECURE)) {
									sofia_glue_add_crypto(tech_pvt, tech_pvt->remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
									switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_RECV, tech_pvt->crypto_tag,
															  tech_pvt->crypto_type, tech_pvt->remote_raw_key, SWITCH_RTP_KEY_LEN);
								}
								got_crypto++;
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Ignoring unacceptable key\n");
							}
						}
					} else if (!switch_rtp_ready(tech_pvt->rtp_session)) {
						tech_pvt->remote_crypto_key = switch_core_session_strdup(tech_pvt->session, crypto);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set Remote Key [%s]\n", tech_pvt->remote_crypto_key);
						tech_pvt->crypto_tag = crypto_tag;
						got_crypto++;

						if (switch_strlen_zero(tech_pvt->local_crypto_key)) {
							if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_32, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_32);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
							} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_80, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_80);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto Setup Failed!.\n");
							}
						}
					}
				}
			}

			if (got_crypto && !got_avp) {
				switch_channel_set_variable(tech_pvt->channel, SOFIA_CRYPTO_MANDATORY_VARIABLE, "true");
				switch_channel_set_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_VARIABLE, "true");
			}

			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}

		  greed:
			x = 0;

			if (tech_pvt->rm_encoding) {// && !sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
				char *remote_host = tech_pvt->remote_sdp_audio_ip;
				switch_port_t remote_port = tech_pvt->remote_sdp_audio_port;
				int same = 0;

				if (switch_rtp_ready(tech_pvt->rtp_session)) {
					remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
					remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);
				}
				
				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if (switch_strlen_zero(map->rm_encoding) && map->rm_pt < 96) {
						match = (map->rm_pt == tech_pvt->pt) ? 1 : 0;
					} else {
						match = strcasecmp(switch_str_nil(map->rm_encoding), tech_pvt->iananame) ? 0 : 1;
					}

					if (match && connection->c_address && remote_host &&
						!strcmp(connection->c_address, remote_host) && m->m_port == remote_port) {
						same = 1;
					} else {
						same = 0;
						break;
					}
				}

				if (same) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
									  "Our existing sdp is still good [%s %s:%d], let's keep it.\n",
									  tech_pvt->rm_encoding, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
					got_audio = 1;
				} else {
					match = 0;
					got_audio = 0;
				}
			}
			
			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				int32_t i;
				uint32_t near_rate = 0;
				const switch_codec_implementation_t *mimp = NULL, *near_match = NULL;
				const char *rm_encoding;

				if (x++ < skip) {
					continue;
				}

				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}

				if (!te && !strcasecmp(rm_encoding, "telephone-event")) {
					te = tech_pvt->te = (switch_payload_t) map->rm_pt;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf payload to %u\n", te);
					if (tech_pvt->rtp_session) {
						switch_rtp_set_telephony_event(tech_pvt->rtp_session, tech_pvt->te);
					}
				}

				if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) && !cng_pt && !strcasecmp(rm_encoding, "CN")) {
					cng_pt = (switch_payload_t) map->rm_pt;
					if (tech_pvt->rtp_session) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", cng_pt);
						switch_rtp_set_cng_pt(tech_pvt->rtp_session, tech_pvt->cng_pt);
					}
				}

				if (match) {
					if (te && cng_pt) {
						break;
					}
					continue;
				}

				if (greedy) {
					first = mine;
					last = first + 1;
				} else {
					first = 0;
					last = tech_pvt->num_codecs;
				}
				
				if (maxptime && (!ptime || ptime > maxptime)) {
					ptime = maxptime;
				}

				for (i = first; i < last && i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
					uint32_t codec_rate = imp->samples_per_second;
					if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
						continue;
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Compare [%s:%d:%u:%d]/[%s:%d:%u:%d]\n",
									  rm_encoding, map->rm_pt, (int) map->rm_rate, ptime,
									  imp->iananame, imp->ianacode, codec_rate, imp->microseconds_per_packet / 1000);
					if (switch_strlen_zero(map->rm_encoding) && map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						match = strcasecmp(rm_encoding, imp->iananame) ? 0 : 1;
					}

					if (match) {
						if (scrooge) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
											  "Bah HUMBUG! Sticking with %s@%uh@%ui\n", 
											  imp->iananame, imp->samples_per_second, imp->microseconds_per_packet / 1000);
						} else {
							if ((ptime && ptime * 1000 != imp->microseconds_per_packet) || 
								map->rm_rate != codec_rate) {
								near_rate = map->rm_rate;
								near_match = imp;
								match = 0;
								continue;
							}
						}
						mimp = imp;
						break;
					} else {
						match = 0;
					}
				}

				if (!match && near_match) {
					const switch_codec_implementation_t *search[1];
					char *prefs[1];
					char tmp[80];
					int num;
					
					switch_snprintf(tmp, sizeof(tmp), "%s@%uh@%ui", near_match->iananame, near_rate ? near_rate : near_match->samples_per_second, ptime);
					
					prefs[0] = tmp;
					num = switch_loadable_module_get_codecs_sorted(search, 1, prefs, 1);
					
					if (num) {
						mimp = search[0];
					} else {
						mimp = near_match;
					}
					
					if (!maxptime || mimp->microseconds_per_packet / 1000 <= maxptime) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Substituting codec %s@%ui@%uh\n",
										  mimp->iananame, mimp->microseconds_per_packet / 1000, mimp->samples_per_second);
						match = 1;
					} else {
						mimp = NULL;
						match = 0;
					}
					
				}

				if (!match && greedy) {
					skip++;
					continue;
				}

				if (mimp) {
					char tmp[50];
					tech_pvt->rm_encoding = switch_core_session_strdup(session, (char *) map->rm_encoding);
					tech_pvt->iananame = switch_core_session_strdup(session, (char *) mimp->iananame);
					tech_pvt->pt = (switch_payload_t) map->rm_pt;
					tech_pvt->rm_rate = map->rm_rate;
					if (!strcasecmp((char *) mimp->iananame, "ilbc") && switch_strlen_zero((char*)map->rm_fmtp)) {
						/* default to 30 when no mode is defined for ilbc ONLY */
						tech_pvt->codec_ms = 30;
					} else {
						tech_pvt->codec_ms = mimp->microseconds_per_packet / 1000;
					}
					tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(session, (char *) connection->c_address);
					tech_pvt->rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);
					tech_pvt->remote_sdp_audio_port = (switch_port_t) m->m_port;
					tech_pvt->agreed_pt = (switch_payload_t) map->rm_pt;
					switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
					switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
					switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);

				}

				if (match) {
					if (sofia_glue_tech_set_codec(tech_pvt, 1) != SWITCH_STATUS_SUCCESS) {
						match = 0;
					}
				}
			}

			if (!match && greedy && mine < tech_pvt->num_codecs) {
				mine++;
				skip = 0;
				goto greed;
			}

		} else if (m->m_type == sdp_media_video && m->m_port) {
			sdp_rtpmap_t *map;
			const char *rm_encoding;
			int framerate = 0;
			const switch_codec_implementation_t *mimp = NULL;
			int vmatch = 0, i;
			switch_channel_set_variable(tech_pvt->channel, "video_possible", "true");

			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}

			for (map = m->m_rtpmaps; map; map = map->rm_next) {

				for (attr = m->m_attributes; attr; attr = attr->a_next) {
					if (!strcasecmp(attr->a_name, "framerate") && attr->a_value) {
						framerate = atoi(attr->a_value);
					}
				}
				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}

				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video Codec Compare [%s:%d]/[%s:%d]\n",
									  rm_encoding, map->rm_pt, imp->iananame, imp->ianacode);
					if (map->rm_pt < 96) {
						vmatch = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						vmatch = strcasecmp(rm_encoding, imp->iananame) ? 0 : 1;
					}


					if (vmatch && (map->rm_rate == imp->samples_per_second)) {
						mimp = imp;
						break;
					} else {
						vmatch = 0;
					}
				}


				if (mimp) {
					if ((tech_pvt->video_rm_encoding = switch_core_session_strdup(session, (char *) rm_encoding))) {
						char tmp[50];
						tech_pvt->video_pt = (switch_payload_t) map->rm_pt;
						tech_pvt->video_rm_rate = map->rm_rate;
						tech_pvt->video_codec_ms = mimp->microseconds_per_packet / 1000;
						tech_pvt->remote_sdp_video_ip = switch_core_session_strdup(session, (char *) connection->c_address);
						tech_pvt->video_rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);
						tech_pvt->remote_sdp_video_port = (switch_port_t) m->m_port;
						tech_pvt->video_agreed_pt = (switch_payload_t) map->rm_pt;
						switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_video_port);
						switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_VIDEO_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
						switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_VIDEO_PORT_VARIABLE, tmp);
						switch_channel_set_variable(tech_pvt->channel, "sip_video_fmtp", tech_pvt->video_rm_fmtp);
						switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->video_agreed_pt);
						switch_channel_set_variable(tech_pvt->channel, "sip_video_pt", tmp);
						sofia_glue_check_video_codecs(tech_pvt);
						break;
					} else {
						vmatch = 0;
					}
				}
			}
		}
	}
	
 done:
	tech_pvt->cng_pt = cng_pt;
	sofia_set_flag_locked(tech_pvt, TFLAG_SDP);

	return match;
}

/* map sip responses to QSIG cause codes ala RFC4497 section 8.4.4 */
switch_call_cause_t sofia_glue_sip_cause_to_freeswitch(int status)
{
	switch (status) {
	case 200:
		return SWITCH_CAUSE_NORMAL_CLEARING;
	case 401:
	case 402:
	case 403:
	case 407:
	case 603:
		return SWITCH_CAUSE_CALL_REJECTED;
	case 404:
	case 485:
	case 604:
		return SWITCH_CAUSE_NO_ROUTE_DESTINATION;
	case 408:
	case 504:
		return SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE;
	case 410:
		return SWITCH_CAUSE_NUMBER_CHANGED;
	case 413:
	case 414:
	case 416:
	case 420:
	case 421:
	case 423:
	case 505:
	case 513:
		return SWITCH_CAUSE_INTERWORKING;
	case 480:
		return SWITCH_CAUSE_NO_USER_RESPONSE;
	case 400:
	case 481:
	case 500:
	case 503:
		return SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE;
	case 486:
	case 600:
		return SWITCH_CAUSE_USER_BUSY;
	case 484:
		return SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
	case 488:
	case 606:
		return SWITCH_CAUSE_INCOMPATIBLE_DESTINATION;
	case 502:
		return SWITCH_CAUSE_NETWORK_OUT_OF_ORDER;
	case 405:
		return SWITCH_CAUSE_SERVICE_UNAVAILABLE;
	case 406:
	case 415:
	case 501:
		return SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED;
	case 482:
	case 483:
		return SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR;
	case 487:
		return SWITCH_CAUSE_ORIGINATOR_CANCEL;
	default:
		return SWITCH_CAUSE_NORMAL_UNSPECIFIED;
	}
}

void sofia_glue_pass_sdp(private_object_t *tech_pvt, char *sdp)
{
	const char *val;
	switch_core_session_t *other_session;
	switch_channel_t *other_channel;

	if ((val = switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE))
		&& (other_session = switch_core_session_locate(val))) {
		other_channel = switch_core_session_get_channel(other_session);
		switch_channel_set_variable(other_channel, SWITCH_B_SDP_VARIABLE, sdp);

		if (!sofia_test_flag(tech_pvt, TFLAG_CHANGE_MEDIA) && (switch_channel_test_flag(other_channel, CF_OUTBOUND) &&
																switch_channel_test_flag(tech_pvt->channel, CF_OUTBOUND) &&
																switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE))) {
			switch_ivr_nomedia(val, SMF_FORCE);
			sofia_set_flag_locked(tech_pvt, TFLAG_CHANGE_MEDIA);
		}
		switch_core_session_rwunlock(other_session);
	}
}

char *sofia_glue_get_url_from_contact(char *buf, uint8_t to_dup)
{
	char *url = NULL, *e;

	if ((url = strchr(buf, '<')) && (e = strchr(url, '>'))) {
		url++;
		if (to_dup) {
			url = strdup(url);
			e = strchr(url, '>');
		}

		*e = '\0';
	} else {
		if (to_dup) {
			url = strdup(buf);
		} else {
			url = buf;
		}
	}
	return url;
}

sofia_profile_t *sofia_glue_find_profile__(const char *file, const char *func, int line, const char *key)
{
	sofia_profile_t *profile;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if ((profile = (sofia_profile_t *) switch_core_hash_find(mod_sofia_globals.profile_hash, key))) {
		if (!sofia_test_pflag(profile, PFLAG_RUNNING)) {
#ifdef SOFIA_DEBUG_RWLOCKS
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is not running\n", profile->name);
#endif
			profile = NULL;
			goto done;
		}
		if (switch_thread_rwlock_tryrdlock(profile->rwlock) != SWITCH_STATUS_SUCCESS) {
#ifdef SOFIA_DEBUG_RWLOCKS
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is locked\n", profile->name);
#endif
			profile = NULL;
		}
	} else {
#ifdef SOFIA_DEBUG_RWLOCKS
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is not in the hash\n", key);
#endif
	}
#ifdef SOFIA_DEBUG_RWLOCKS
	if (profile) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX LOCK %s\n", profile->name);
	}
#endif

  done:
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return profile;
}

void sofia_glue_release_profile__(const char *file, const char *func, int line, sofia_profile_t *profile)
{
	if (profile) {
#ifdef SOFIA_DEBUG_RWLOCKS
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX UNLOCK %s\n", profile->name);
#endif
		switch_thread_rwlock_unlock(profile->rwlock);
	}
}

switch_status_t sofia_glue_add_profile(char *key, sofia_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (!switch_core_hash_find(mod_sofia_globals.profile_hash, key)) {
		status = switch_core_hash_insert(mod_sofia_globals.profile_hash, key, profile);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return status;
}

void sofia_glue_del_gateway(sofia_gateway_t *gp)
{
	if (!gp->deleted) {
		if (gp->state != REG_STATE_NOREG) {
			gp->retry = 0;
			gp->state = REG_STATE_UNREGISTER;
		}

		gp->deleted = 1;
	}
}

void sofia_glue_restart_all_profiles(void) 
{
	switch_hash_index_t *hi;
	const void *var;
    void *val;
    sofia_profile_t *pptr;
	switch_xml_t xml_root;
	const char *err;

	if ((xml_root = switch_xml_open_root(1, &err))) {
		switch_xml_free(xml_root);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Reload XML [%s]\n", err);
	}

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
        for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
            if ((pptr = (sofia_profile_t *) val)) {
				int rsec = 10;
				int diff = (int) (switch_epoch_time_now(NULL) - pptr->started);
				int remain = rsec - diff;
				if (sofia_test_pflag(pptr, PFLAG_RESPAWN) || !sofia_test_pflag(pptr, PFLAG_RUNNING)) {
					continue;
				}

				if (diff < rsec) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
									  "Profile %s must be up for at least %d seconds to stop/restart.\nPlease wait %d second%s\n",
									  pptr->name, rsec, remain, remain == 1 ? "" : "s");
					continue;
				}
				sofia_set_pflag_locked(pptr, PFLAG_RESPAWN);
				sofia_clear_pflag_locked(pptr, PFLAG_RUNNING);
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}

void sofia_glue_del_profile(sofia_profile_t *profile)
{
	sofia_gateway_t *gp;
	char *aliases[512];
	int i = 0, j = 0;
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val) && pptr == profile) {
				aliases[i++] = strdup((char *) var);
				if (i == 512) {
					abort();
				}
			}
		}

		for (j = 0; j < i && j < 512; j++) {
			switch_core_hash_delete(mod_sofia_globals.profile_hash, aliases[j]);
			free(aliases[j]);
		}

		for (gp = profile->gateways; gp; gp = gp->next) {
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gp->name);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gp->register_from);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gp->register_contact);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "deleted gateway %s\n", gp->name);
			profile->gateways = NULL;
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}

int sofia_glue_init_sql(sofia_profile_t *profile)
{
	char *test_sql = NULL;

	char reg_sql[] =
		"CREATE TABLE sip_registrations (\n"
		"   call_id         VARCHAR(255),\n"
		"   sip_user        VARCHAR(255),\n"
		"   sip_host        VARCHAR(255),\n"
		"   presence_hosts  VARCHAR(255),\n"
		"   contact         VARCHAR(1024),\n"
		"   status          VARCHAR(255),\n"
		"   rpid            VARCHAR(255),\n" 
		"   expires         INTEGER,\n" 
		"   user_agent      VARCHAR(255),\n" 
		"   server_user     VARCHAR(255),\n"
		"   server_host     VARCHAR(255),\n" 
		"   profile_name    VARCHAR(255),\n" 
		"   hostname        VARCHAR(255),\n" 
		"   network_ip      VARCHAR(255),\n" 
		"   network_port    VARCHAR(6),\n" 
		"   sip_username    VARCHAR(255),\n" 
		"   sip_realm       VARCHAR(255),\n"
		"   mwi_user        VARCHAR(255),\n"
		"   mwi_host        VARCHAR(255)\n"
		");\n";


	char pres_sql[] =
		"CREATE TABLE sip_presence (\n"
		"   sip_user        VARCHAR(255),\n"
		"   sip_host        VARCHAR(255),\n"
		"   status          VARCHAR(255),\n"
		"   rpid            VARCHAR(255),\n" 
		"   expires         INTEGER,\n" 
		"   user_agent      VARCHAR(255),\n" 
		"   profile_name    VARCHAR(255),\n" 
		"   hostname        VARCHAR(255),\n" 
		"   network_ip      VARCHAR(255),\n" 
		"   network_port    VARCHAR(6)\n" 
		");\n";

	char dialog_sql[] =
		"CREATE TABLE sip_dialogs (\n"
		"   call_id         VARCHAR(255),\n"
		"   uuid            VARCHAR(255),\n"
		"   sip_to_user     VARCHAR(255),\n"
		"   sip_to_host     VARCHAR(255),\n"
		"   sip_from_user   VARCHAR(255),\n"
		"   sip_from_host   VARCHAR(255),\n"
		"   contact_user    VARCHAR(255),\n"
		"   contact_host    VARCHAR(255),\n"
		"   state           VARCHAR(255),\n" 
		"   direction       VARCHAR(255),\n" 
		"   user_agent      VARCHAR(255),\n" 
		"   profile_name    VARCHAR(255),\n"
        "   hostname        VARCHAR(255)\n"
		");\n";

	char sub_sql[] =
		"CREATE TABLE sip_subscriptions (\n"
		"   proto           VARCHAR(255),\n"
		"   sip_user        VARCHAR(255),\n"
		"   sip_host        VARCHAR(255),\n"
		"   sub_to_user     VARCHAR(255),\n"
		"   sub_to_host     VARCHAR(255),\n"
		"   presence_hosts  VARCHAR(255),\n"
		"   event           VARCHAR(255),\n"
		"   contact         VARCHAR(1024),\n"
		"   call_id         VARCHAR(255),\n"
		"   full_from       VARCHAR(255),\n"
		"   full_via        VARCHAR(255),\n"
		"   expires         INTEGER,\n" 
		"   user_agent      VARCHAR(255),\n" 
		"   accept          VARCHAR(255),\n"
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255),\n"
		"   network_port    VARCHAR(6),\n"
		"   network_ip      VARCHAR(255)\n"
		");\n";

	char auth_sql[] = 
		"CREATE TABLE sip_authentication (\n" 
		"   nonce           VARCHAR(255),\n" 
		"   expires         INTEGER," 
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255)\n"
		");\n";

	/* should we move this glue to sofia_sla or keep it here where all db init happens? XXX MTK */
	char shared_appearance_sql[] = 
		"CREATE TABLE sip_shared_appearance_subscriptions (\n"
		"   subscriber        VARCHAR(255),\n"
		"   call_id           VARCHAR(255),\n"
		"   aor               VARCHAR(255),\n"
		"   profile_name      VARCHAR(255),\n"
		"   hostname          VARCHAR(255),\n"
		"   contact_str       VARCHAR(255),\n"
		"   network_ip        VARCHAR(255)\n" 
		");\n";

	char shared_appearance_dialogs_sql[] = 
		"CREATE TABLE sip_shared_appearance_dialogs (\n"
		"   profile_name      VARCHAR(255),\n"
		"   hostname          VARCHAR(255),\n"
		"   contact_str       VARCHAR(255),\n"
		"   call_id           VARCHAR(255),\n"
		"   network_ip        VARCHAR(255),\n"
		"   expires           INTEGER\n"
		");\n";

	if (switch_odbc_available() && profile->odbc_dsn) {
		int x;
		char *indexes[] = {
			"create index sr_call_id on sip_registrations (call_id)",
			"create index sr_sip_user on sip_registrations (sip_user)",
			"create index sr_sip_host on sip_registrations (sip_host)",
			"create index sr_profile_name on sip_registrations (profile_name)",
			"create index sr_presence_hosts on sip_registrations (presence_hosts)",
			"create index sr_contact on sip_registrations (contact)",
			"create index sr_expires on sip_registrations (expires)",
			"create index sr_hostname on sip_registrations (hostname)",
			"create index sr_status on sip_registrations (status)",
			"create index sr_network_ip on sip_registrations (network_ip)",
			"create index sr_network_port on sip_registrations (network_port)",
			"create index sr_sip_username on sip_registrations (sip_username)",
			"create index sr_sip_realm on sip_registrations (sip_realm)",
			"create index ss_call_id on sip_subscriptions (call_id)",
			"create index ss_hostname on sip_subscriptions (hostname)",
			"create index ss_hostname on sip_subscriptions (network_ip)",
			"create index ss_sip_user on sip_subscriptions (sip_user)",
			"create index ss_sip_host on sip_subscriptions (sip_host)",
			"create index ss_presence_hosts on sip_subscriptions (presence_hosts)",
			"create index ss_event on sip_subscriptions (event)",
			"create index ss_proto on sip_subscriptions (proto)",
			"create index ss_sub_to_user on sip_subscriptions (sub_to_user)",
			"create index ss_sub_to_host on sip_subscriptions (sub_to_host)",
			"create index sd_uuid on sip_dialogs (uuid)",
			"create index sd_hostname on sip_dialogs (hostname)",
			"create index sp_hostname on sip_presence (hostname)",
			"create index sa_nonce on sip_authentication (nonce)",
			"create index sa_hostname on sip_authentication (hostname)",
			"create index ssa_hostname on sip_shared_appearance_subscriptions (hostname)",
			"create index ssa_hostname on sip_shared_appearance_subscriptions (network_ip)",
			"create index ssa_subscriber on sip_shared_appearance_subscriptions (subscriber)",
			"create index ssa_profile_name on sip_shared_appearance_subscriptions (profile_name)",
			"create index ssa_aor on sip_shared_appearance_subscriptions (aor)",
			"create index ssd_profile_name on sip_shared_appearance_dialogs (profile_name)",
			"create index ssd_hostname on sip_shared_appearance_dialogs (hostname)",
			"create index ssd_contact_str on sip_shared_appearance_dialogs (contact_str)",
			"create index ssd_call_id on sip_shared_appearance_dialogs (call_id)",
			"create index ssd_expires on sip_shared_appearance_dialogs (expires)",
			NULL	
		};

		if (!(profile->master_odbc = switch_odbc_handle_new(profile->odbc_dsn, profile->odbc_user, profile->odbc_pass))) {
			return 0;
		}
		if (switch_odbc_handle_connect(profile->master_odbc) != SWITCH_ODBC_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Connecting ODBC DSN: %s\n", profile->odbc_dsn);
			return 0;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected ODBC DSN: %s\n", profile->odbc_dsn);
		
		test_sql = switch_mprintf("delete from sip_registrations where (contact like '%%TCP%%' "
								  "or status like '%%TCP%%' or status like '%%TLS%%') and hostname='%q' "
								  "and network_ip!='-1' and network_port!='-1' and sip_username != '-1' and mwi_user != '-1' and mwi_host != '-1'", 
								  mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_registrations", NULL);
			switch_odbc_handle_exec(profile->master_odbc, reg_sql, NULL);
		}
		free(test_sql);


		test_sql = switch_mprintf("delete from sip_subscriptions where hostname='%q' and network_ip!='-1' and network_port!='-1'", mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_subscriptions", NULL);
			switch_odbc_handle_exec(profile->master_odbc, sub_sql, NULL);
		}

		free(test_sql);
		test_sql = switch_mprintf("delete from sip_dialogs where hostname='%q'", mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_dialogs", NULL);
			switch_odbc_handle_exec(profile->master_odbc, dialog_sql, NULL);
		}

		test_sql = switch_mprintf("delete from sip_presence where hostname='%q'", mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_presence", NULL);
			switch_odbc_handle_exec(profile->master_odbc, pres_sql, NULL);
		}

		free(test_sql);
		test_sql = switch_mprintf("delete from sip_authentication where hostname='%q'", mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_authentication", NULL);
			switch_odbc_handle_exec(profile->master_odbc, auth_sql, NULL);
		}
		free(test_sql);

		test_sql = switch_mprintf("delete from sip_shared_appearance_subscriptions where contact_str='' or hostname='%q' and network_ip!='-1'",
								  mod_sofia_globals.hostname);
		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_shared_appearance_subscriptions", NULL);
			switch_odbc_handle_exec(profile->master_odbc, shared_appearance_sql, NULL);
		}
		free(test_sql);


		test_sql = switch_mprintf("delete from sip_shared_appearance_dialogs where contact_str='' or hostname='%q' and network_ip!='-1'",
								  mod_sofia_globals.hostname);
		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_shared_appearance_dialogs", NULL);
			switch_odbc_handle_exec(profile->master_odbc, shared_appearance_dialogs_sql, NULL);
		}
		free(test_sql);


		for (x = 0; indexes[x]; x++) {
			switch_odbc_handle_exec(profile->master_odbc, indexes[x], NULL);
		}


	} else if (profile->odbc_dsn) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
	} else {
		if (!(profile->master_db = switch_core_db_open_file(profile->dbname))) {
			return 0;
		}

		test_sql = switch_mprintf("delete from sip_registrations where (contact like '%%TCP%%' "
								  "or status like '%%TCP%%' or status like '%%TLS%%') and hostname='%q' "
								  "and network_ip!='-1' and network_port!='-1' and sip_username != '-1' and mwi_user != '-1' and mwi_host != '-1'",
								  mod_sofia_globals.hostname);
		
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_registrations", reg_sql);
		free(test_sql);

		test_sql = switch_mprintf("delete from sip_subscriptions where hostname='%q' and network_ip!='-1'", mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_subscriptions", sub_sql);
		free(test_sql);

		test_sql = switch_mprintf("delete from sip_dialogs where hostname='%q'", mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_dialogs", dialog_sql);
		free(test_sql);

		test_sql = switch_mprintf("delete from sip_presence where hostname='%q'", mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_presence", pres_sql);
		free(test_sql);

		test_sql = switch_mprintf("delete from sip_authentication where hostname='%q'", mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_authentication", auth_sql);
		free(test_sql);

		
		test_sql = switch_mprintf("delete from sip_shared_appearance_subscriptions where contact_str = '' or hostname='%q' and network_ip!='-1'",
								  mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_shared_appearance_subscriptions", shared_appearance_sql);
		free(test_sql);

		test_sql = switch_mprintf("delete from sip_shared_appearance_dialogs where contact_str = '' or hostname='%q'", mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_shared_appearance_dialogs", shared_appearance_dialogs_sql);
		free(test_sql);
		
		switch_core_db_exec(profile->master_db, "create index if not exists ssa_hostname on sip_shared_appearance_subscriptions (hostname)", 
							NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ssa_subscriber on sip_shared_appearance_subscriptions (subscriber)", 
							NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ssa_profile_name on sip_shared_appearance_subscriptions (profile_name)", 
							NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ssa_aor on sip_shared_appearance_subscriptions (aor)", NULL, NULL, NULL);
		

		switch_core_db_exec(profile->master_db, "create index if not exists ssd_profile_name on sip_shared_appearance_dialogs (profile_name)", 
							NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ssd_hostname on sip_shared_appearance_dialogs (hostname)", 
							NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ssd_hostname on sip_shared_appearance_dialogs (network_ip)", 
							NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ssd_contact_str on sip_shared_appearance_dialogs (contact_str)",  
							NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ssd_call_id on sip_shared_appearance_dialogs (call_id)",  
							NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ssd_expires on sip_shared_appearance_dialogs (expires)", 
							NULL, NULL, NULL);
		


		switch_core_db_exec(profile->master_db, "create index if not exists sr_call_id on sip_registrations (call_id)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_sip_user on sip_registrations (sip_user)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_sip_host on sip_registrations (sip_host)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_profile_name on sip_registrations (profile_name)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_presence_hosts on sip_registrations (presence_hosts)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_contact on sip_registrations (contact)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_expires on sip_registrations (expires)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_hostname on sip_registrations (hostname)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_status on sip_registrations (status)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_network_ip on sip_registrations (network_ip)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_network_port on sip_registrations (network_port)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_sip_username on sip_registrations (sip_username)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sr_sip_realm on sip_registrations (sip_realm)", NULL, NULL, NULL);


		switch_core_db_exec(profile->master_db, "create index if not exists ss_call_id on sip_subscriptions (call_id)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_hostname on sip_subscriptions (hostname)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_hostname on sip_subscriptions (network_ip)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_sip_user on sip_subscriptions (sip_user)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_sip_host on sip_subscriptions (sip_host)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_presence_hosts on sip_subscriptions (presence_hosts)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_event on sip_subscriptions (event)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_proto on sip_subscriptions (proto)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_sub_to_user on sip_subscriptions (sub_to_user)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists ss_sub_to_host on sip_subscriptions (sub_to_host)", NULL, NULL, NULL);

		switch_core_db_exec(profile->master_db, "create index if not exists sd_uuid on sip_dialogs (uuid)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sd_hostname on sip_dialogs (hostname)", NULL, NULL, NULL);

		switch_core_db_exec(profile->master_db, "create index if not exists sp_hostname on sip_presence (hostname)", NULL, NULL, NULL);

		switch_core_db_exec(profile->master_db, "create index if not exists sa_nonce on sip_authentication (nonce)", NULL, NULL, NULL);
		switch_core_db_exec(profile->master_db, "create index if not exists sa_hostname on sip_authentication (hostname)", NULL, NULL, NULL);
	}

	if (switch_odbc_available() && profile->odbc_dsn) {
		return profile->master_odbc ? 1 : 0;
	}

	return profile->master_db ? 1 : 0;
}

void sofia_glue_sql_close(sofia_profile_t *profile)
{
	if (switch_odbc_available() && profile->master_odbc) {
		switch_odbc_handle_destroy(&profile->master_odbc);
	} else {
		switch_core_db_close(profile->master_db);
		profile->master_db = NULL;
	}
}

void sofia_glue_execute_sql(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *d_sql = NULL, *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;

	if (profile->sql_queue) {
		if (sql_already_dynamic) {
			d_sql = sql;
		} else {
			d_sql = strdup(sql);
		}

		switch_assert(d_sql);
		if ((status = switch_queue_trypush(profile->sql_queue, d_sql)) == SWITCH_STATUS_SUCCESS) {
			d_sql = NULL;
		}
	} else if (sql_already_dynamic) {
		d_sql = sql;
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		sofia_glue_actually_execute_sql(profile, SWITCH_FALSE, sql, profile->ireg_mutex);
	}

	switch_safe_free(d_sql);

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}
}

void sofia_glue_actually_execute_sql(sofia_profile_t *profile, switch_bool_t master, char *sql, switch_mutex_t *mutex)
{
	switch_core_db_t *db;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (switch_odbc_available() && profile->odbc_dsn) {
		switch_odbc_statement_handle_t stmt;
		if (switch_odbc_handle_exec(profile->master_odbc, sql, &stmt) != SWITCH_ODBC_SUCCESS) {
			char *err_str;
			err_str = switch_odbc_handle_get_error(profile->master_odbc, stmt);
			if (!switch_strlen_zero(err_str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, err_str);
			}
			switch_safe_free(err_str);
		}
		switch_odbc_statement_handle_free(&stmt);
	} else if (profile->odbc_dsn) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
	} else {
		if (master) {
			db = profile->master_db;
		} else {
			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				goto end;
			}
		}
		switch_core_db_persistant_execute(db, sql, 1);

		if (!master) {
			switch_core_db_close(db);
		}
	}

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}
}

switch_bool_t sofia_glue_execute_sql_callback(sofia_profile_t *profile,
											  switch_bool_t master, switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	switch_core_db_t *db;
	char *errmsg = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}


	if (switch_odbc_available() && profile->odbc_dsn) {
		switch_odbc_handle_callback_exec(profile->master_odbc, sql, callback, pdata);
	} else if (profile->odbc_dsn) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
	} else {

		if (master) {
			db = profile->master_db;
		} else {
			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				goto end;
			}
		}

		switch_core_db_exec(db, sql, callback, pdata, &errmsg);

		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
			free(errmsg);
		}

		if (!master && db) {
			switch_core_db_close(db);
		}
	}

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}
	return ret;
}

static char *sofia_glue_execute_sql2str_odbc(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;

	if (switch_odbc_handle_exec_string(profile->master_odbc, sql, resbuf, len) == SWITCH_ODBC_SUCCESS) {
		ret = resbuf;
	}

	return ret;
}


char *sofia_glue_execute_sql2str(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	switch_core_db_t *db;
	switch_core_db_stmt_t *stmt;
	char *ret = NULL;

	if (switch_odbc_available() && profile->odbc_dsn) {
		return sofia_glue_execute_sql2str_odbc(profile, mutex, sql, resbuf, len);
	}

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(db = switch_core_db_open_file(profile->dbname))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
		goto end;
	}

	if (switch_core_db_prepare(db, sql, -1, &stmt, 0)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Statement Error [%s]!\n", sql);
		goto fail;
	} else {
		int running = 1;
		int colcount;

		while (running < 5000) {
			int result = switch_core_db_step(stmt);
			const unsigned char *txt;

			if (result == SWITCH_CORE_DB_ROW) {
				if ((colcount = switch_core_db_column_count(stmt)) > 0) {
					if ((txt = switch_core_db_column_text(stmt, 0))) {
						switch_copy_string(resbuf, (char *) txt, len);
						ret = resbuf;
					} else {
						goto fail;
					}
				}
				break;
			} else if (result == SWITCH_CORE_DB_BUSY) {
				running++;
				switch_cond_next();
				continue;
			}
			break;
		}

		switch_core_db_finalize(stmt);
	}

  fail:
	switch_core_db_close(db);

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}

int sofia_glue_get_user_host(char *in, char **user, char **host)
{
	char *p = NULL, *h = NULL, *u = in;

	if (!in) {
		return 0;
	}

	/* First isolate the host part from the user part */
	if ((h = strchr(u, '@'))) {
		*h++ = '\0';
	}

	/* Clean out the user part of its protocol prefix (if any) */
	if ((p = strchr(u, ':'))) {
		*p++ = '\0';
		u = p;
	}

	/* Clean out the host part of any suffix */
	if (h) {
		if ((p = strchr(h, ':'))) {
			*p = '\0';
		}

		if ((p = strchr(h, ';'))) {
			*p = '\0';
		}

		if ((p = strchr(h, ' '))) {
			*p = '\0';
		}
	}
	if (user) {
		*user = u;
	}
	if (host) {
		*host = h;
	}

	return 1;
}

const char *sofia_glue_strip_proto(const char *uri)
{
	char *p;

	if ((p = strchr(uri, ':'))) {
		return p+1;
	}

	return uri;
}

sofia_cid_type_t sofia_cid_name2type(const char *name)
{
	if (!strcasecmp(name, "rpid")) {
		return CID_TYPE_RPID;
	}

	if (!strcasecmp(name, "pid")) {
		return CID_TYPE_PID;
	}

	return CID_TYPE_NONE;
	
}

/* all the values of the structure are initialized to NULL  */
/* in case of failure the function returns NULL */
/* sofia_destination->route can be NULL */
sofia_destination_t* sofia_glue_get_destination(char *data)
{
	sofia_destination_t *dst = NULL;
	char *to = NULL;
	char *contact = NULL;
    char *route = NULL;
    char *route_uri = NULL;
	char *eoc = NULL;
	char *p = NULL;

	if (switch_strlen_zero(data)) {
		return NULL;
	}

	if (!(dst = (sofia_destination_t *)malloc(sizeof(sofia_destination_t)))) {
		return NULL;
	}

	/* return a copy of what is in the buffer between the first < and > */
	if (!(contact = sofia_glue_get_url_from_contact(data, 1))) {
		goto mem_fail;
	}

	if((eoc = strstr(contact, ";fs_path="))) {
		*eoc = '\0';

		if(!(route = strdup(eoc + 9))) {
			goto mem_fail;
		}

		for (p = route; p && *p ; p++) {
			if (*p == '>' || *p == ';') {
				*p = '\0';
				break;
			}
		}

		switch_url_decode(route);

 		if (!(route_uri = strdup(route))) {
			goto mem_fail;
		}
		if ((p = strchr(route_uri, ','))) {
			do {
				*p = '\0';
			} while ((--p > route_uri) && *p == ' ');
		}
	} 
	
	if (!(to = strdup(data))) {
		goto mem_fail;
	}

	if((eoc = strstr(to, ";fs_path="))) {
		*eoc++ = '>';	
		*eoc = '\0';	
	}
	
	if ((p = strstr(contact, ";fs_"))) {
		*p = '\0';
	}
	
	dst->contact = contact;
	dst->to = to;
	dst->route = route;
	dst->route_uri = route_uri;
	return dst;

mem_fail:
	switch_safe_free(contact);
	switch_safe_free(to);
	switch_safe_free(route);
	switch_safe_free(route_uri);
	switch_safe_free(dst);
	return NULL;
}

void sofia_glue_free_destination(sofia_destination_t *dst)
{
	if (dst) {
		switch_safe_free(dst->contact);
		switch_safe_free(dst->route);
		switch_safe_free(dst->route_uri);
		switch_safe_free(dst->to);
		switch_safe_free(dst);
	}
}

switch_status_t sofia_glue_send_notify(sofia_profile_t *profile, const char *user, const char *host, const char *event, const char *contenttype, const char *body, const char *o_contact, const char *network_ip)
{
	char *id = NULL;
	nua_handle_t *nh;
	sofia_destination_t *dst = NULL;
	char *contact_str, *contact, *user_via = NULL;
	char *route_uri = NULL;

	contact = sofia_glue_get_url_from_contact((char*) o_contact, 1);
	if (sofia_glue_check_nat(profile, network_ip)) {
		char *ptr = NULL;
		const char *transport_str = NULL;


		id = switch_mprintf("sip:%s@%s", user, profile->extsipip);
		switch_assert(id);

		if ((ptr = sofia_glue_find_parameter(o_contact, "transport="))) {
			sofia_transport_t transport = sofia_glue_str2transport(ptr);
			transport_str = sofia_glue_transport2str(transport);
			switch (transport) {
				case SOFIA_TRANSPORT_TCP:
					contact_str = profile->tcp_public_contact;
					break;
				case SOFIA_TRANSPORT_TCP_TLS:
					contact_str = profile->tls_public_contact;
					break;
				default:
					contact_str = profile->public_url;
					break;
			}
			user_via = sofia_glue_create_external_via(NULL, profile, transport);
		} else {
			user_via = sofia_glue_create_external_via(NULL, profile, SOFIA_TRANSPORT_UDP);
			contact_str = profile->public_url;
		}

	} else {
		contact_str = profile->url;
		id = switch_mprintf("sip:%s@%s", user, host);
	}

	dst = sofia_glue_get_destination((char*) o_contact);
	switch_assert(dst);

	if(dst->route_uri) {
		route_uri = sofia_glue_strip_uri(dst->route_uri);
	}

	nh = nua_handle(profile->nua, NULL, NUTAG_URL(contact),
			SIPTAG_FROM_STR(id), SIPTAG_TO_STR(id),
			SIPTAG_CONTACT_STR(contact_str), TAG_END());
	nua_handle_bind(nh, &mod_sofia_globals.destroy_private);

	nua_notify(nh,
			NUTAG_NEWSUB(1),
			TAG_IF(dst->route_uri, NUTAG_PROXY(route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)),
			TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
			SIPTAG_EVENT_STR(event),
			SIPTAG_CONTENT_TYPE_STR(contenttype),
			SIPTAG_PAYLOAD_STR(body), TAG_END());

	switch_safe_free(contact);
	switch_safe_free(route_uri);
	switch_safe_free(id);
	sofia_glue_free_destination(dst);
	switch_safe_free(user_via);

	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
