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


## Implementing .ktx2 into the asset loading portion.

- [x] Did it.

- Had to add some extra cmake code to get it to include resources into the app bundle:
    > Ref: https://discourse.cmake.org/t/how-to-add-resources-to-macos-bundle/9323/2


## Implementing shaders into the asset loading portion.

- [x] Write a slang shader.


## Compiling the raw assets.

```sh
# For compiling a JPG to KTX2.
ktx create --format R8G8B8A8_SRGB assets_raw/textures/default_tex.jpg assets/textures/default_tex.ktx2

# For compiling slang shader to SPIR-V (obfuscated and high optimization).
slangc -lang slang -profile glsl_460 -target spirv -reflection-json assets/shaders/default_shader.shadrefl -O2 -obfuscate -fvk-use-entrypoint-name assets_raw/shaders/default_shader.slang > assets/shaders/default_shader.shader
```

> @NOTE: `-fvk-use-entrypoint-name` is required so single-entrypoint shaders (esp. compute shaders) don't get their entrypoint renamed to "main()".


There could be a python script that stores the file hashes for all files that have been there and will rebuild everything that is added/deleted/changed. It would just have to check the file hash to see if it changed.

For shaders, it could be compiled into the obfuscated, optimized SPIR-V and then also gt the reflection-json ~~and then also create a glsl version so that it's easy for me to understand?~~

~~For material-specific shaders, there could be a check for the bindless material-param element.~~ Or this could just be assumed.

For images, there could be a file suffix that says what type of image it is.
Example:

"default_tex.2d.mip-n.jpg" → "default_tex.ktx2" (2D, no mipmapping)

"default_tex.cubemap-px.mip-y.jpg" ┐
"default_tex.cubemap-nx.mip-y.jpg" ┤
"default_tex.cubemap-py.mip-y.jpg" ┤
"default_tex.cubemap-ny.mip-y.jpg" ┤
"default_tex.cubemap-pz.mip-y.jpg" ┤
"default_tex.cubemap-nz.mip-y.jpg" ┴→ "default_tex.ktx2" (cubemap, with mipmapping)

"default_tex.3d-0.jpg" ┐
"default_tex.3d-1.jpg" ┤
"default_tex.3d-2.jpg" ┴→ "default_tex.ktx2" (3D)

"default_tex.2darray-0.jpg" ┐
"default_tex.2darray-1.jpg" ┤
"default_tex.2darray-2.jpg" ┴→ "default_tex.ktx2" (2D array)


### Bindless descriptors.

This seems like a nice thing for bindless texture access.

https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/03-convenience-features.html#descriptorhandle-for-bindless-descriptor-access


## Going back to a prev convo: implementing shader loading into the thing.


slangc -lang slang -profile glsl_460 -target spirv -reflection-json assets/shaders/gradient.shadrefl -O2 -obfuscate -fvk-use-entrypoint-name assets_raw/shaders/gradient.slang > assets/shaders/gradient.shader

> @NOTE: `-fvk-use-entrypoint-name` is required so single-entrypoint shaders (esp. compute shaders) don't get their entrypoint renamed to "main()".

slangc -lang slang -profile glsl_460 -target glsl assets_raw/shaders/gradient.slang > assets/shaders/gradient.glsl


- [ ] I want these to be reflected:
    - [ ] Descriptor set layouts
        - [x] Have the information for that in `extract_stuff()`
        - [ ] It would be good to get a descriptor layout cache thing.
    - [ ] Pipeline layouts (kind of an extension of the descriptor set layouts)
        - [x] Have the information for that in `extract_stuff()`
        - I think it should be enough, since there just needs to be the entrypoint name and the descriptor layout things and which descriptor layouts go to which pipeline layouts.
    - [ ] Actual descriptor sets.
        - [ ] Need to get material params working.
            - Connect the textures and buffers and also just in general various parameters.
                - There may need to be an extra layer for buffers of material params for things like indirect drawing and that kinda shit. Crazy stuff that may need to be deferred in the future and for now just have one material per material param?
                    - In the future i def want to make it so that there's that extra abstraction.
        - [ ] Connect the material param textures and buffers (convert from string to actual material handles).
        - [ ] Create and update the descriptor sets (should just be needed once unless using a resizable texture).

    > How can I connect descriptor sets to the descriptor set layouts and then to the pipelines??
        > Expose these things as shader params?
            > Create descriptor sets from the shader params in materials???
    > Hardcode stuff for now!

- [ ] Perhaps no reflection?
    - This is a lot harder than I had thought it would be.
    - Really the only thing I was wanting reflection for was creating multiple material sets, but honestly that probably isn't even relevant? I'm taking the super-long way here aaaahhhh.
    - Maybe the interface would be the same: input material name, shader involved, material params.
        - Model's inside material names or material sets connect to material name.
        - Shader is referenced.
        - Material params defines the props set to the shader.
    - Really this will only be used for vertex-fragment graphics pipeline shaders that will use the geometry.
        - So I don't need to add support for compute shaders either!!!
        - So then I feel like it really isn't necessary to even try to tackle this with reflection, especially since there's only going to be a handful of shaders:
            - pbr-opaque
            - pbr-transparent
            - water (this might just be a specialty shader??)
            - ???
        - BUT!!!! it could be made easier for shaders to be made!
            - Perhaps just inputting the wanted bindings for descriptor set layout and then calling it good there is all that's needed?
                - The descriptor set layout can be made.
                - The pipeline layout can be made.
                - The pipeline can be made.

                - Descriptor sets and push constants would have to be provided.
                    - This could be made easier as well, using the pipeline layout information.

                - Material params would manipulate buffers and such that the descriptor sets are using.

                - Just everything being able to be bound with a simple `.bind(draw_func_lambda)` would be really nice.
                    - transition involved images to ATTACHMENT, or SHADER_READ_ONLY. (do buffers have to be transitioned??)
                    - bind pipeline.
                    - attach descriptor sets
                    - run `draw_func_lambda()` which could have a vkDraw or a vkIndirectDraw.
                    - record what happened to relevant images for future transitions.



    - Compute shaders can have a really simple interface too where you can set things up and then run `.compute()`, and it will:
        - transition involved images to GENERAL (and buffers too?)
        - bind pipeline
        - attach descriptor sets
        - dispatch compute shader (include params in `compute()` in case there's things like screen size, etc that's needed... or that can pull from an injected dependency?)
        - record on relevant images (and buffers too?) what happened, so they know how to transition next if they do again.

    - Mmmm I think reflection a little bit can be used! Just don't want to implement every single way this could be used and have the most flexible system everrrrr kinda thing.
        - So asserting that the `thread_group_size` is the assumed size is important and reflection would help with that!

    - [x] Did a partial implementation of this.


- [ ] Get a hardcoded render pipeline for geometry going!!! (needed for prereq for "Perhaps no reflection?")
    - [x] Iron out the data structures.
    - [x] Load all models.
        - [x] stubs
        - [x] Actual load in.
            - It should load in the static model, then the skinning data, then the material data.
            - Animation data should also get loaded in.
                - Should there be the animation re-recording as well?
                    - I don't really think it was particularly important, and plus, having the animation speed change would be nice, but it would still need to be deterministic.
                        - But hey, it was a system that worked. And idk if I wanna try to redesign a different system.
                        - Ig I'll keep using it until it proves itself to be good or bad.
                        - One way I could do an irregular framerate is to just have a float counter for the simulation thread, and if multiple frames are skipped over, just make sure to simulate for all the in-between frames.
            - [x] Create similar animation data.
                - [x] Load in animation data.
        - [x] Figure out how the data shuold get returned.
            - It just gets input into a data collection (pool-likes)
    - [ ] Upload buffer for geometry.
        - [x] Set up the vertex offset stuff.
        - [ ] (for static meshes) Combine the vertex and index buffers into one big one
            > @NOTE: keep the vertices/indices so that physics engine can access it.
        - [ ] Set all the `default_per_mesh_vertex_index_offset_set`s up.
        - [ ] Load it up onto staging buffer and upload to gpu (for static meshes).
    - [ ] Create render pipeline.
    - [ ] 
