/** @file
 *  @brief HoG Service sample
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HOG_H
#define HOG_H
#ifdef __cplusplus
extern "C" {
#endif

void hog_init(void);

void hog_button_loop(void);

extern const struct gpio_dt_spec statusLed;
extern const struct gpio_dt_spec actionLed;

extern volatile bool buttons_active;

#ifdef __cplusplus
}
#endif
#endif
