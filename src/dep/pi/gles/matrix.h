/**
 * Copyright (C) 2015 Akop Karapetyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PHL_MATRIX_H
#define PHL_MATRIX_H

struct phl_matrix {
	float xx, xy, xz, xw;
	float yx, yy, yz, yw;
	float zx, zy, zz, zw;
	float wx, wy, wz, ww;
};

void phl_matrix_identity(struct phl_matrix *m);
void phl_matrix_copy(struct phl_matrix *dst, const struct phl_matrix *src);
void phl_matrix_multiply(struct phl_matrix *r,
	const struct phl_matrix *a, const struct phl_matrix *b);
void phl_matrix_scale(struct phl_matrix *m, float x, float y, float z);
void phl_matrix_rotate_x(struct phl_matrix *m, float angle);
void phl_matrix_rotate_y(struct phl_matrix *m, float angle);
void phl_matrix_rotate_z(struct phl_matrix *m, float angle);
void phl_matrix_ortho(struct phl_matrix *m,
	float left, float right, float bottom, float top, float near, float far);
void phl_matrix_dump(const struct phl_matrix *m);

#endif // PHL_MATRIX_H
