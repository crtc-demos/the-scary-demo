#include <ogcsys.h>
#include <gccore.h>

#include "matrixutil.h"

void
matrixutil_swap_yz (Mtx src, Mtx dst)
{
  float tx, ty, tz;
  tx = guMtxRowCol (src, 0, 1);
  ty = guMtxRowCol (src, 1, 1);
  tz = guMtxRowCol (src, 2, 1);
  guMtxCopy (src, dst);
  guMtxRowCol (dst, 0, 1) = guMtxRowCol (src, 0, 2);
  guMtxRowCol (dst, 1, 1) = guMtxRowCol (src, 1, 2);
  guMtxRowCol (dst, 2, 1) = guMtxRowCol (src, 2, 2);
  guMtxRowCol (dst, 0, 2) = tx;
  guMtxRowCol (dst, 1, 2) = ty;
  guMtxRowCol (dst, 2, 2) = tz;
}
