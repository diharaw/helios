#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace lumen
{
	class Camera
	{
	public:
		glm::mat4 m_view;
		glm::mat4 m_projection;
		glm::mat4 m_view_projection;
		glm::mat4 m_inv_view;
		glm::mat4 m_inv_projection;
		glm::mat4 m_inv_view_projection;

		glm::vec3 m_position;
		glm::vec3 m_target;
		glm::vec3 m_up;

		float m_fov;
		float m_aspect_ratio;
		float m_near_plane;
		float m_far_plane;

		void set_projection(float fov, float aspect_ratio, float near_plane, float far_plane)
		{
			m_fov = fov;
			m_aspect_ratio = aspect_ratio;
			m_near_plane = near_plane;
			m_far_plane = far_plane;
		}

		void set_orientation(glm::vec3 position, glm::vec3 target, glm::vec3 up)
		{
			m_position = position;
			m_target = target;
			m_up = up;
		}

		void update()
		{
			m_projection = glm::perspective(glm::radians(m_fov), m_aspect_ratio, m_near_plane, m_far_plane);
			m_view = glm::lookAt(m_position, m_target, m_up);
			m_view_projection = m_projection * m_view;

			m_inv_view = glm::inverse(m_view);
			m_inv_projection = glm::inverse(m_projection);
			m_inv_view_projection = glm::inverse(m_view_projection);
		}
	};
}