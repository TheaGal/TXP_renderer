# Hawsoo devnotes

## Simple design/usage of the render engine.

```cpp
TXP::Renderer r{ "Renderer Title Here" };

std::vector<Render_obj_create_config> configs{
    {
        .layer = RENDER_LAYER_DEFAULT,
        .model_name = "SlimeGirl",
        .material_set = "SlimeGirl_ms2",  // Overrides the default material set.
        .deform_config = {
            .animator_template = "SlimeGirl.btanitor",
            .anim_frame_action_control = "SlimeGirl.btafa",
        },
    },
    {
        .layer = RENDER_LAYER_DEFAULT,
        .model_name = "Prop01",
    },
};

std::vector<TXP::pool_key_t> rend_obj_keys =
    r.create_render_objs_from_config(std::move(configs));

// Do a bunch of stuff here.

r.destroy_render_objs(std::move(rend_obj_keys));
```

~~It seems pretty similar to the checkout-return pattern in the BTZC game engine's renderer.~~
Mmm so no actually. This is a create-destroy lifetime pattern, so very different.

`.create_render_objs_from_config()` is meant to be called from the simulation loop/thread, so it will block as little as possible.


### For simulation loop.
```cpp
// Inside sim loop.
r.simulation_handle().animator_set_state_set(obj_key0, my_state_set0);
r.simulation_handle().jump_queue_emplace_state_set(obj_key1,
                                                   "jq_midair",
                                                   my_state_set1);
r.simulation_handle().jump_queue_clear(obj_key2, "jq_grnd_mvt");
r.simulation_handle().jump_queue_set_state_set(obj_key3, "jq_midair", my_state_set3);
r.simulation_handle().update_all_animators();
```

This just simply shows that to explicitly handle the renderer's animators in the simulation loop, you just use `.simulation_handle()` to access the simulation-loop-specific functions. These are dedicated to the simulation loop (or thread) so it's limited to the only things one needs.





### Thoughts
> So it really just seems like the only things public interface about this lib is going to be for the simulation handle. For the rendering side, it's just going to be ctor'ing the renderer and then setting up the config files and then setting it off.

> Mmm, it does seem like there's some stuff going on with whether the window should show up, or when to compile shaders and stuff.

> The renderer will have to also take care of polling for input, returning the input, and then also camera modes.
    > And then also the ImGui renderer stuff, however, that seems like it needs some kind of callback to hook into for imgui stuff in the game engine portion.
    > If the imgui stuff is in a callback, then stuff like `.change_camera_type(FLY_CAM)` should be exposed in the public ifc.


> Everything in the public interface really should be taken care of with some kind of multithreading-friendly systme (or ig thread-safe is the word lol).


> It really just looks like the actual public interface would be `renderer.h` and that's it!!



## Initializing Vulkan 1.3 on macOS.

Basic premise is having a .cpp file that is completely culled out if not win64 or darwin, so yay!

And then creating the vulkan renderer (ideally it should be identical for these two platforms code-wise).

Making abstractions along the way is the goal, since I don't want to be stuck with any vulkan syntax inside the actual `renderer.cpp` by the end.
    > GOALS HERE!!!

> Ahhhhhhhh I just (re-found out) found out that glfw (and all windowing code for that matter) MUST RUN ON THE MAIN THREAD. Fwick. That sucks. Well, ig that means that simulation thread can run on a separate thread then?
