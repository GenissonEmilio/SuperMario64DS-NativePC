//cpp
/* Copy the camera matrix into the live view, then invert it.
 *
 * leftovers:
 * - data_0209b3ec is the view matrix and data_0209b41c is its inverse.
 *   Both stay linker names; no header owns them.
 * - Matrix4x3 is the flat 12-int form from common.h. math/Matrix.h spells
 *   the same 0x30 bytes as .r/.t, and this TU does not include it.
 */
#include "common.h"

extern "C" {
extern Matrix4x3 data_0209b3ec;
extern Matrix4x3 data_0209b41c;
extern void InvMat4x3(Matrix4x3* dst, Matrix4x3* inv);
}

void CopyToViewMat(const Matrix4x3* src)
{
    data_0209b3ec = *src;
    InvMat4x3(&data_0209b3ec, &data_0209b41c);
}
