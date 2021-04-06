/**
 * @file outlier_detect.c
 * @note Copyright (C) 2013 Henry Jesuiter <henry.jesuiter@alcnetworx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "print.h"
#include "msg.h"
#include "pi.h"

#include "outlier_detect.h"
#include "filter_private.h"

#define NSEC_PER_SEC 1000000000

struct outlier_detector {
	struct filter filter;   // base "class"

	double max_offset;	// defined offset limit for servo
	tmv_t limit;		// current offset limit (defined by middle value of 'sample' array)

	size_t len;		// size of '*order' and '*samples' array
	size_t cnt;		// current number samples in '*samples'  ('cnt' <= 'len')

	int index;		// next index to be changed in 'samples' ('idx' <= 'cnt')
	int *order;		// sorted values stored as index of 'samples'
	tmv_t *samples;		// array of samples to be used inside this filter as circular buffer
};

/*
 * private local functions
 */

static int update_sorted_array(struct outlier_detector *od) {
	unsigned i;

	if (od->cnt < od->len) {
		od->cnt++;
	} else {
		/* remove index of the replaced value from order. */
		for (i = 0; i < od->cnt - 1; i++) {
			// look for the oldest element (thats the one to replace) in 'od->order'
			if (od->order[i] == od->index) {
				break;
			}
		}
		for (; i + 1 < od->cnt; i++) {
			// overwrite it by copying all successors one element ahead
			// Note: this causes the last element to be doubled
			od->order[i] = od->order[i + 1];
		}
	}

	/* insert index of the new value to order. */
	for (i = od->cnt - 1; i > 0; i--) {
		// now find where to place the new 'sample' in 'order', we are searching from tail here
		if (od->samples[od->order[i - 1]] <= od->samples[od->index]) {
			break;
		}
		// push elements that are to big one element back
		// Note: this "repairs" the doubled last element, either by overwriting the last element below, or by overwriting one of its predecessors
		od->order[i] = od->order[i - 1];
	}
	// now put the new element to the appropriate place
	od->order[i] = od->index;

	return (1 + od->index) % od->len; // next element in order to be replaced
}

/*
 * Interface implementation starts here
 */

static void outlier_detect_destroy(struct filter *filter)
{
	struct outlier_detector *od = container_of(filter, struct outlier_detector, filter);
	free(od->samples);
	free(od->order);
	free(od);
}

static void outlier_detect_reset(struct filter *filter)
{
	struct outlier_detector *od = container_of(filter, struct outlier_detector, filter);

	od->cnt = 0;
	od->index = 0;

	od->limit = od->max_offset;

	memset(od->order, 0, od->len);
	memset(od->samples, 0, od->len);
}

static tmv_t outlier_detect_sample(struct filter *filter, tmv_t sample)
{
	struct outlier_detector *od = container_of(filter, struct outlier_detector, filter);

	// add new sample to circular buffer
	od->samples[od->index] = llabs(sample);

	// update sorted array of 'sample'-buffer
	od->index = update_sorted_array(od);

	if (od->cnt % 2) {
		// odd number of samples, so 'limit' is the middle value in 'samples'
		od->limit = od->samples[od->order[od->cnt / 2]];
	} else {
		// even number of samples, so 'limit' is the average of the both middle values in 'samples'
		od->limit = tmv_div(tmv_add(od->samples[od->order[od->cnt / 2 - 1]], od->samples[od->order[od->cnt / 2]]), 2);
	}

	//pr_info("sample %10" PRId64 " current limit %10" PRId64,tmv_to_nanoseconds(sample), tmv_to_nanoseconds(od->limit));

	if (tmv_to_nanoseconds(llabs(sample)) > od->max_offset) {
		// servo will go unlocked
		outlier_detect_reset(filter);

		return sample;   // während einlauf keine beeinflussung von sample
	}

	// servo in locked state
	// check if out of limit
	if (llabs(sample) > od->limit) {
		return (sample < 0) ? -(od->limit) : od->limit;
	}

	return sample; // innerhalb des limits keine beeinflussung von sample
}

struct filter *outlier_detect_create(int length, double threshold)
{
	struct outlier_detector *od;

	if (length < 1) {
		return NULL;
	}

	od = calloc(1, sizeof(struct outlier_detector));
	if (!od) {
		return NULL;
	}

	// implement filter interface here
	od->filter.destroy = outlier_detect_destroy;
	od->filter.sample = outlier_detect_sample;
	od->filter.reset = outlier_detect_reset;

	od->max_offset = 1.0 * NSEC_PER_SEC;
	if (threshold > 0.0) {
		od->max_offset = threshold * NSEC_PER_SEC;
	}
	//pr_info("caught: %f %f!", configured_pi_offset, od->max_offset);

	// prepare outlier detector data structures
	od->order = calloc(1, length * sizeof(*od->order));
	if (!od->order) {
		free(od);
		return NULL;
	}

	od->samples = calloc(1, length * sizeof(*od->samples));
	if (!od->samples) {
		free(od->order);
		free(od);
		return NULL;
	}

	od->len = length;

	return &od->filter;
}
