#include "window.h"
#include "renderer.h"
#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "log.h"

#define WIDTH 1024
#define HEIGHT 768

struct frame_time_t
{
    clock_t curr;
    clock_t prev;
    clock_t delta;
};

inline clock_t get_fps(frame_time_t& time)
{
    clock_t fps = 0;
    if (time.delta)
    {
        fps = CLOCKS_PER_SEC/time.delta;
    }
    return fps;
}

inline f32 get_delta_seconds(frame_time_t& time)
{
    return time.delta/static_cast<f32>(CLOCKS_PER_SEC);
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
        add_light(scene, vec3(0, 5, 4), vec3(1));
        add_light(scene, vec3(1, 2, 1), vec3(0,0,1));
        add_light(scene, vec3(-2, 1, 4), vec3(1,0,0));
    }

    camera.position = vec3(0, 2, 1);
    camera.lookat(vec3(0, 0, 4));

    // render loop
    frame_time_t time{0,0,0};
    bool run = true;
    while(run)
    {
        time.curr  = clock();
        time.delta = time.curr - time.prev;
        f32 dt = get_delta_seconds(time);
        if (dt >= 0.01667) 
        {
            scene.entities[1].m_model *= rotate4x4_y(dt);
            time.prev = time.curr;
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
            }

            renderer.draw_scene(scene, camera);
        }
    }

    return 0;
}
