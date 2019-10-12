/**
** Copyright (C) 2015 Akop Karapetyan
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**/

#ifndef PHL_UDEV_H
#define PHL_UDEV_H

int phl_udev_init();
int phl_udev_shutdown();

int phl_udev_joy_count();
int phl_udev_mouse_count();

const char* phl_udev_joy_name(int index);
const char* phl_udev_joy_id(int index);

#endif // PHL_UDEV_H
