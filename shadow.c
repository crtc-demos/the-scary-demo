#include <stdlib.h>
#include <math.h>

#include "shadow.h"
#include "utility-texture.h"

/* Make a ramp texture. RAMPBITS can be 8 or 16.  */

shadow_info *
create_shadow_info (int rampbits, light_info *light)
{
  shadow_info *shinf;
  
  shinf = calloc (1, sizeof (shadow_info));
  
  switch (rampbits)
    {
    case 8:
      shinf->ramp_type = UTIL_TEX_8BIT_RAMP;
      break;

    case 16:
      shinf->ramp_type = UTIL_TEX_16BIT_RAMP;
      break;

    default:
      abort ();
    }

  shinf->light = light;

  return shinf;
}

void
destroy_shadow_info (shadow_info *shinf)
{
  free_utility_texture (shinf->ramp_type);
  free (shinf);
}

void
shadow_set_bounding_radius (shadow_info *shinf, float radius)
{
  shinf->target_radius = radius;
}

void
shadow_setup_ortho (shadow_info *shinf, float near, float far)
{
  float radius = shinf->target_radius;
  Mtx lightmat, proj;
  Mtx dp = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
  float range = far - near, tscale;

  if (shinf->ramp_type == UTIL_TEX_8BIT_RAMP)
    tscale = 16.0f;
  else
    tscale = 256.0f;

  guLightOrtho (proj, -radius, radius, -radius, radius, 0.5f, 0.5f, 0.5f, 0.5f);
  
  /* We need:
      s = - (z + N) / ( F - N )
      t = s * tscale
  */
  guMtxRowCol (dp, 0, 2) = -1.0f / range;
  guMtxRowCol (dp, 0, 3) = -near / range;
  guMtxRowCol (dp, 1, 2) = guMtxRowCol (dp, 0, 2) * tscale;
  guMtxRowCol (dp, 1, 3) = guMtxRowCol (dp, 0, 3) * tscale;
  guMtxRowCol (dp, 2, 3) = 1.0f;
  
  guLookAt (lightmat, &shinf->light->pos, &shinf->light->up,
	    &shinf->light->lookat);

  guMtxConcat (proj, lightmat, shinf->shadow_tex_projection);
  guMtxConcat (dp, lightmat, shinf->shadow_tex_depth);
  
  guOrtho (shinf->shadow_projection, radius, -radius, -radius, radius, near,
	   far);
  guMtxCopy (lightmat, shinf->light_cam);

  shinf->projection_type = GX_ORTHOGRAPHIC;
}

void
shadow_setup_frustum (shadow_info *shinf, float near, float far)
{
  float radius = shinf->target_radius;
  Mtx lightmat, proj;
  Mtx dp = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
  float range = far - near, tscale;
  float light_to_obj_dist_sq, aperture;
  light_info *light = shinf->light;
  
  if (shinf->ramp_type == UTIL_TEX_8BIT_RAMP)
    tscale = 16.0f;
  else
    tscale = 256.0f;

  /* Proved... with maths!  */
  light_to_obj_dist_sq = light->pos.x * light->pos.x
			 + light->pos.y * light->pos.y
			 + light->pos.z * light->pos.z;

  aperture = near * sqrtf (light_to_obj_dist_sq - radius * radius)
	     / sqrtf (light_to_obj_dist_sq);

  guLightFrustum (proj, -aperture, aperture, -aperture, aperture, near,
		  0.5f, 0.5f, 0.5f, 0.5f);

  guMtxRowCol (dp, 0, 2) = far / range;
  guMtxRowCol (dp, 0, 3) = far * near / range;
  guMtxRowCol (dp, 1, 2) = guMtxRowCol (dp, 0, 2) * tscale;
  guMtxRowCol (dp, 1, 3) = guMtxRowCol (dp, 0, 3) * tscale;
  guMtxRowCol (dp, 2, 2) = 1.0f;

  guLookAt (lightmat, &shinf->light->pos, &shinf->light->up,
	    &shinf->light->lookat);

  guMtxConcat (proj, lightmat, shinf->shadow_tex_projection);
  guMtxConcat (dp, lightmat, shinf->shadow_tex_depth);

  guFrustum (shinf->shadow_projection, aperture, -aperture, -aperture, aperture,
	     near, far);
  guMtxCopy (lightmat, shinf->light_cam);

  shinf->projection_type = GX_PERSPECTIVE;
}

void
shadow_casting_tev_setup (void)
{
#include "null.inc"
}
