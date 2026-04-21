/* SPDX-License-Identifier: MIT */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 256

void config_defaults(struct aec_config *cfg)
{
	cfg->enabled = true;
	cfg->far_gain = 1.0f;
	cfg->near_gain = 1.0f;
	cfg->delay_ms = 0;
	cfg->nlp_mode = 1;
	cfg->metrics = false;
	cfg->delay_logging = false;
	cfg->delay_agnostic = true;
	cfg->extended_filter = true;
	cfg->drift_compensation = false;

	cfg->user_mode = false;
	cfg->mu_min = 0.000001f;
	cfg->mu_decay = 0.9f;
	cfg->cor_thd1 = 0.98f;
	cfg->cor_thd2 = 0.95f;
	cfg->cor_thd3 = 0.9f;
	cfg->cor_thd4 = 0.8f;
	cfg->far_pow_thd = 800000.0f;
	cfg->safe_suppression = 0.1f;
	cfg->restrain_band_center = 0;
	cfg->restrain_band_wide = 0;
	cfg->restrain_factor = 0.0f;
}

static char *strip(char *s)
{
	while (isspace((unsigned char)*s))
		s++;
	size_t len = strlen(s);
	if (len > 0) {
		char *end = s + len - 1;
		while (end > s && isspace((unsigned char)*end))
			*end-- = '\0';
	}
	return s;
}

static bool parse_bool(const char *val)
{
	return strcmp(val, "true") == 0 || strcmp(val, "1") == 0;
}

static int parse_suppression_mode(const char *val)
{
	if (strstr(val, "Aggressive") || strstr(val, "aggressive"))
		return 2;
	if (strstr(val, "Conservative") || strstr(val, "conservative"))
		return 0;
	return 1;
}

int config_load(const char *path, struct aec_config *cfg)
{
	config_defaults(cfg);

	if (!path || !*path)
		return 0;

	FILE *f = fopen(path, "r");
	if (!f)
		return -1;

	char line[MAX_LINE];
	enum {
		SEC_NONE,
		SEC_FAR_FRAME,
		SEC_NEAR_FRAME,
		SEC_AEC,
		SEC_OTHER
	} section = SEC_NONE;

	while (fgets(line, sizeof(line), f)) {
		char *s = strip(line);
		if (*s == '\0' || *s == '#' || *s == ';')
			continue;

		if (*s == '[') {
			char *end = strchr(s, ']');
			if (end)
				*end = '\0';
			s++;

			if (strcmp(s, "Set_Far_Frame") == 0)
				section = SEC_FAR_FRAME;
			else if (strcmp(s, "Set_Near_Frame") == 0)
				section = SEC_NEAR_FRAME;
			else if (strcmp(s, "AEC") == 0)
				section = SEC_AEC;
			else
				section = SEC_OTHER;
			continue;
		}

		char *eq = strchr(s, '=');
		if (!eq)
			continue;

		*eq = '\0';
		char *key = strip(s);
		char *val = strip(eq + 1);

		switch (section) {
		case SEC_FAR_FRAME:
			if (strcmp(key, "Frame_V") == 0)
				cfg->far_gain = (float)atof(val);
			break;

		case SEC_NEAR_FRAME:
			if (strcmp(key, "Frame_V") == 0)
				cfg->near_gain = (float)atof(val);
			else if (strcmp(key, "delay_ms") == 0)
				cfg->delay_ms = atoi(val);
			break;

		case SEC_AEC:
			if (strcmp(key, "AEC_enable") == 0)
				cfg->enabled = parse_bool(val);
			else if (strcmp(key, "enable_drift_compensation") == 0)
				cfg->drift_compensation = parse_bool(val);
			else if (strcmp(key, "set_suppression_mode") == 0)
				cfg->nlp_mode = (int16_t)atoi(val);
			else if (strcmp(key, "set_suppression_level") == 0)
				cfg->nlp_mode = (int16_t)parse_suppression_mode(val);
			else if (strcmp(key, "enable_metrics") == 0)
				cfg->metrics = parse_bool(val);
			else if (strcmp(key, "enable_delay_logging") == 0)
				cfg->delay_logging = parse_bool(val);
			else if (strcmp(key, "aec_mode") == 0)
				cfg->user_mode = (strcmp(val, "user") == 0);
			else if (strcmp(key, "set_mu_min") == 0)
				cfg->mu_min = (float)atof(val);
			else if (strcmp(key, "set_mu_decay") == 0)
				cfg->mu_decay = (float)atof(val);
			else if (strcmp(key, "set_cor_thd1") == 0)
				cfg->cor_thd1 = (float)atof(val);
			else if (strcmp(key, "set_cor_thd2") == 0)
				cfg->cor_thd2 = (float)atof(val);
			else if (strcmp(key, "set_cor_thd3") == 0)
				cfg->cor_thd3 = (float)atof(val);
			else if (strcmp(key, "set_cor_thd4") == 0)
				cfg->cor_thd4 = (float)atof(val);
			else if (strcmp(key, "set_far_pow_thd") == 0)
				cfg->far_pow_thd = (float)atof(val);
			else if (strcmp(key, "set_safe_suppression_value") == 0)
				cfg->safe_suppression = (float)atof(val);
			else if (strcmp(key, "set_restrain_band_center") == 0)
				cfg->restrain_band_center = atoi(val);
			else if (strcmp(key, "set_restrain_band_wide") == 0)
				cfg->restrain_band_wide = atoi(val);
			else if (strcmp(key, "set_restrain_factor") == 0)
				cfg->restrain_factor = (float)atof(val);
			break;

		default:
			break;
		}
	}

	fclose(f);
	return 0;
}
