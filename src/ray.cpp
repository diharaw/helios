#include "ray.h"

namespace lumen
{
	Ray Ray::compute(float x, float y, float tmin, float tmax, const Camera& camera)
	{
		x = x * 2.0 - 1.0;
		y = y * 2.0 - 1.0;

		glm::vec4 clip_pos = glm::vec4(x, y, -1.0, 1.0);
		glm::vec4 view_pos = camera.m_inv_projection * clip_pos;

		glm::vec3 dir = glm::vec3(camera.m_inv_view * glm::vec4(view_pos.x, view_pos.y, -1.0, 0.0));
		dir = normalize(dir);

		glm::vec4 origin = camera.m_inv_view * glm::vec4(0.0, 0.0, 0.0, 1.0);
		origin /= origin.w;

		Ray r;

		r.origin = origin;
		r.dir = dir;
		r.tmax = tmax;
		r.tmin = tmin;

		return r;
	}
}