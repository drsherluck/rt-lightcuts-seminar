#include "window.h"
#include "renderer.h"
#include "mesh.h"
#include "scene.h"
#include "camera.h"
#include "log.h"
#include "time.h"
#include "ui.h"

#include <algorithm>

#define WIDTH 1280
#define HEIGHT 960

#define USE_RANDOM_LIGHTS 0
#define DISTANCE_FROM_ORIGIN 1
#define RANDOM_LIGHT_COUNT 1<<10//(1 << 17) // 17 is around 100000 lights (sorting worse after this)

static v3 random_color()
{
    float r = _randf();
    if (r <= 0.33333f)
    {
        return vec3(1,0,0);
    }
    else if (r <= 0.6667f)
    {
        return vec3(0,1,0);
    }
    return vec3(0,0,1);
}

static void add_random_lights(scene_t& scene, u32 count, v3 origin, f32 distance)
{
    for (u32 i = 0; i < count; ++i)
    {
        v3 color = random_color();//vec3(_randf(), _randf(), _randf());
        v3 dir = normalize(vec3(_randf2(), _randf(), _randf2()));
        v3 pos = origin + dir * distance;
        add_light(scene, pos, color);
    }
}

static void move_lights_from_origin(scene_t& scene, v3 origin, f32 distance)
{
    v4 orig  = vec4(origin, 0);
    for (auto& light : scene.lights)
    {
        v4 dir = normalize(light.pos - vec4(origin, 1));
        light.pos = orig + dir * distance;
    }
}

static void move_lights(scene_t& scene, v4 t)
{
    for (auto& light : scene.lights) 
    {
        light.pos += t;
    }
}

static void add_default_lights(scene_t& scene) 
{
    add_light(scene, vec3(2, 1, -2), vec3(1, 0, 0));
    add_light(scene, vec3(1, 2, 2), vec3(0, 1, 0));
    add_light(scene, vec3(-2, 4, -1), vec3(0, 0, 1));
    add_light(scene, vec3(-2, 3, 2), vec3(1, 1, 1));
}

int main()
{
    window_t window;
    window.create_window("Engine", WIDTH, HEIGHT);
    renderer_t renderer(&window); 


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
        add_entity(scene, 0, 0, translate4x4(0, 0, 0));
        add_entity(scene, 0, 0, translate4x4(0, 1, 0) * scale4x4(0.5) * rotate4x4_y(radians(45)));
        add_entity(scene, 1, 1, translate4x4(0, -0.5, 0) * scale4x4(2));
#if !USE_RANDOM_LIGHTS
        add_default_lights(scene);
#else
        add_random_lights(scene, RANDOM_LIGHT_COUNT, vec3(0), DISTANCE_FROM_ORIGIN);
#endif
        std::sort(std::begin(scene.entities), std::end(scene.entities),
                [](const entity_t& a, const entity_t& b) 
                { return a.mesh_id < b.mesh_id; });

        create_acceleration_structures(renderer.context, scene);
    }

    // create descriptor sets;
    renderer.update_descriptors(scene);

    f32 aspect = window.get_aspect_ratio();
    camera_t camera;
    camera.set_perspective(radians(60.0f), aspect);
    camera.position = vec3(0, 5, -10);
    camera.lookat(vec3(0));
    camera.update(0, window);

    camera_t topdown;
    topdown.set_orthographic(7.0f, aspect);
    topdown.position = vec3(0, 5, 0);
    topdown.lookat(vec3(0));
    topdown.update(0, window);
    topdown.freeze();
    
    // render loop
    frame_time_t time;
    bool run = true;
    bool pause = false;

    render_state_t render_state;
    render_state.cut = (cut_t*) malloc(sizeof(cut_t) * MAX_LIGHT_TREE_SIZE);
    camera_t *curr_camera = &camera;
    while(run)
    {
        update_time(time, 1.0/144.0);
        f32 dt = delta_in_seconds(time);
        run = window.poll_events();

        render_state_t prev = render_state;
        bool imgui_mouse = new_frame(renderer.imgui, &render_state, &scene, time);
        if (prev.use_random_lights != render_state.use_random_lights || render_state.num_random_lights != prev.num_random_lights)
        {
            scene.lights.clear();
            if (render_state.use_random_lights)
            {
                add_random_lights(scene, render_state.num_random_lights, vec3(0), render_state.distance_from_origin);
            }
            else
            {
                add_default_lights(scene);
            }
        }
        // move lighs from origin if it was changed
        if (render_state.distance_from_origin != prev.distance_from_origin)
        {
            move_lights_from_origin(scene, vec3(0), render_state.distance_from_origin);
        }

        if (is_key_pressed(window, KEY_ESC))
        {
            window.close();
            run = false;
        }
        if (is_key_pressed(window, KEY_P))
        {
            print_time_info(time);
            pause = !pause;
            LOG_INFO(pause ? "paused" : "resumed");
        }
        if (is_key_pressed(window, KEY_0))
        {
            render_state.render_depth_buffer = !render_state.render_depth_buffer;
        }
        if (is_key_pressed(window, KEY_R))
        {
            render_state.render_sample_lines = true;
            render_state.screen_uv = window.input_manager.curr_mouse_pos;
            v2 screen_uv = floor(vec2(WIDTH, HEIGHT) * render_state.screen_uv);
            LOG_INFO("(%f, %f)", screen_uv.x, screen_uv.y);
        }
        if (is_key_pressed(window, KEY_Q))
        {
            if (curr_camera == &camera)
            {
                curr_camera = &topdown;
            }
            else
            {
                curr_camera = &camera;
            }
        }

        if (is_key_pressed(window, KEY_1))
        {
            render_state.render_step_mode = !render_state.render_step_mode;
            render_state.step = 0;
        }
        if (is_key_pressed(window, KEY_J))
        {
            if (render_state.render_step_mode)
            {
                render_state.step = MAX(render_state.step - 1, 0);
            }
        }
        if (is_key_pressed(window, KEY_K))
        {
            if (render_state.render_step_mode)
            {
                render_state.step++;
            }
        }
        if (is_key_pressed(window, KEY_2)) 
        {
            render_state.render_only_selected_nodes = !render_state.render_only_selected_nodes;
        }
    
        if (!pause) 
        {
            // update camera only if ui is not clicked 
            if (!imgui_mouse)
            {
                curr_camera->update(dt, window);
            }
            if (is_key_down(window, KEY_ARROW_UP))
            {
                if (!is_key_down(window, KEY_SHIFT_L))
                {
                    move_lights(scene, vec4(0, 2 * dt, 0, 0));
                } 
                else
                {
                    move_lights(scene, vec4(0, 0, 2 * dt, 0));
                }
            }
            if (is_key_down(window, KEY_ARROW_DOWN))
            {
                if (!is_key_down(window, KEY_SHIFT_L))
                {
                    move_lights(scene, vec4(0, -2 * dt, 0, 0));
                }
                else
                {
                    move_lights(scene, vec4(0, 0, -2 * dt, 0));
                }
            }
            if (is_key_down(window, KEY_ARROW_RIGHT))
            {
                move_lights(scene, vec4(2*dt, 0, 0, 0));
            }
            if (is_key_down(window, KEY_ARROW_LEFT))
            {
                move_lights(scene, vec4(-2*dt, 0, 0, 0));
            }

            if (is_key_pressed(window, KEY_EQUAL)) 
            {
                render_state.num_samples = MIN(render_state.num_samples + 1, scene.lights.size());
            }
            if (is_key_pressed(window, KEY_MINUS)) 
            {
                render_state.num_samples = MAX(render_state.num_samples - 1, 1);
            }

            scene.entities[1].m_model *= rotate4x4_y(dt);
            update_acceleration_structures(renderer.context, scene);
            renderer.draw_scene(scene, *curr_camera, render_state);
        } 
        else 
        {
            if (is_key_pressed(window, KEY_N))
            {
                scene.entities[1].m_model *= rotate4x4_y(dt);
                update_acceleration_structures(renderer.context, scene);
                renderer.draw_scene(scene, *curr_camera, render_state);
            }
        }
    }

    free(render_state.cut);
    destroy_scene(renderer.context, scene);
    return 0;
}
