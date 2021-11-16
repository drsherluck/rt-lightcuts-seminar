#include "window.h"
#include "renderer.h"
#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "log.h"
#include "time.h"

#include <algorithm>

#define WIDTH 640//1024
#define HEIGHT 480//768

#define _randf() (((f32) rand()/(RAND_MAX)) + 1.0)
#define _randf2() ((((f32) rand()/(RAND_MAX)) + 1.0) * (rand() % 2 ? -1.0 : 1.0))

#define RANDOM_LIGHT_COUNT 100000//(1 << 17) // 17 is around 100000 lights (sorting worse after this)

static void add_random_lights(scene_t& scene, u32 count, v3 origin, f32 max_distance)
{
    for (u32 i = 0; i < count; ++i)
    {
        v3 color = vec3(_randf(), _randf(), _randf());
        v3 pos = vec3(_randf2(), _randf2(), _randf2());
        pos *= max_distance/length(pos);
        pos = origin + pos;
        add_light(scene, pos, color);
    }
}

int main()
{
    window_t window;
    window.create_window("Engine", WIDTH, HEIGHT);
    renderer_t renderer(&window); 

    camera_t camera;
    f32 aspect = window.get_aspect_ratio();
    camera.set_fov(radians(60.0f), aspect);

    // load meshes and scene
    scene_t scene;
    {
        std::vector<mesh_data_t> mesh_data(2);
        load_mesh_data("resources/dragon.obj", mesh_data[0]);

        material_t porcelain;
        porcelain.base_color = vec3(0.9);
        porcelain.roughness = 0.75;
        porcelain.metalness= 0.1;
        porcelain.emissive = 0;

        material_t ground;
        ground.base_color = vec3(1);
        ground.roughness = 1;
        ground.metalness = 0;
        ground.emissive = 0;
       
        plane_mesh_data(mesh_data[1]);
        create_scene(renderer.context, mesh_data, scene);
        add_material(scene, porcelain);
        add_material(scene, ground);
        add_entity(scene, 0, 0, translate4x4(0, 0, 4));
        add_entity(scene, 0, 0, translate4x4(0, 1, 4) * scale4x4(0.5) * rotate4x4_y(radians(45)));
        add_entity(scene, 1, 1, translate4x4(0, -0.5, 4) * scale4x4(2));
        add_light(scene, vec3(1,0,0), vec3(1,1,0));
        add_light(scene, vec3(1,1,1), vec3(1));
        add_light(scene, vec3(0), vec3(1,0,0));
        //add_random_lights(scene, RANDOM_LIGHT_COUNT, vec3(0,0,4), 10);

        std::sort(std::begin(scene.entities), std::end(scene.entities),
                [](const entity_t& a, const entity_t& b) 
                { return a.mesh_id < b.mesh_id; });

        create_acceleration_structures(renderer.context, scene);
    }

    // create descriptor sets;
    renderer.update_descriptors(scene);

    camera.position = vec3(0, 2, 1);
    camera.lookat(vec3(0, 0, 4));
    
    // render loop
    frame_time_t time;
    bool run = true;
    while(run)
    {
        update_time(time);
        f32 dt = delta_in_seconds(time);
        if (1)
        {
            scene.entities[1].m_model *= rotate4x4_y(dt);
            update_acceleration_structures(renderer.context, scene);
            run = window.poll_events();

            camera.update(dt, window);
            if (is_key_pressed(window, KEY_ESC))
            {
                window.close();
                run = false;
            }
            if (is_key_pressed(window, KEY_P))
            {
                renderer.context.allocator.print_memory_usage();
                print_time_info(time);
            }

            renderer.draw_scene(scene, camera);
        }
    }

    destroy_scene(renderer.context, scene);

    return 0;
}
