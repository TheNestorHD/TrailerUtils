#pragma once
#include "physics.h"

#pragma pack(push, 1)
namespace prism
{
	class steering_data_t // Size: 0x0048
	{
	public:
		array_dyn_t<float> steps;
		int32_t total_steps; //0x0020 (0x04)
		int32_t N00003996; //0x0024 (0x04)
		array_dyn_t<float> model_steering_pos; //0x0028 (0x20)
	};
	static_assert(sizeof(steering_data_t) == 0x48);

	// Partial 1.62 layout; the full engine object size is not known.
	class game_trailer_actor_u : public unit_t
	{
	public:
		char pad_0010[0x6B8 - 0x10];
		float steering;                          // 0x06B8
		char pad_06BC[4];
		steering_data_t* steering_data;           // 0x06C0
		char pad_06C8[0xFB0 - 0x6C8];
		physics_joint_physx_t* hook_joint;        // 0x0FB0
		char pad_0FB8[0x1058 - 0xFB8];
		game_trailer_actor_u* slave;              // 0x1058 (candidate)
	};
}
#pragma pack(pop)