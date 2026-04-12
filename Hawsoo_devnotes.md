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


- [ ] ~~I want these to be reflected:~~
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

- [x] Perhaps no reflection?
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


- [x] Get a hardcoded render pipeline for geometry going!!! (needed for prereq for "Perhaps no reflection?")
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
    - [x] Upload buffer for geometry.
        - [x] Set up the vertex offset stuff.
        - [x] (for static meshes) Combine the vertex and index buffers into one big one
            > @NOTE: keep the vertices/indices so that physics engine can access it.
        - [x] Set all the `default_per_mesh_vertex_index_offset_set`s up.
        - [x] Load it up ~~onto staging buffer~~ and upload to gpu (for static meshes).
            > Turns out with rebar/sam in today's word staging buffer isn't necessary! (according to _How to Vulkan in 2026_)
    - [x] Create render pipeline.
        - [x] Create shader.
        - [x] Look back at previous project (solanine-vulkan) to see how dynamic image arrays worked.
        - [x] Implement descriptor set layouts.
            - [x] created a future-catch message to create a material batcher if the texture limit is exceeded.
            - [x] create the combined image sampler descriptor.
            - [x] create shader pipeline object.
            - [x] Ichiou: for the draw() cmd for basic-diffuse shader.
            - [x] Create the environment data buffer (per-frame).
                - [x] Added it as an allocated buffer in per-frame data.
                - [x] Use the buffer's device address in shader.
                - [x] Create the buffer with a proper size.
            - [x] create the model transforms buffer (per-frame).
                - Perhaps it could be just a maximum of the 65535 model transforms?
                - 4 * 16 * 65535 = 4,194,240 (4MB)
                - That times 3 = 12MB
                    - We could have 12MB be allotted for model transforms.
                - [x] Added it as an allocated buffer in per-frame data.
                - [x] Use the buffer's device address in shader.
                - [x] Create the buffer with a proper size.
        - [x] Split out the `load_assets()` func so that it's in this order:
            1. load textures, create all-texture descriptor.
            2. construct shaders
            3. load materials
            4. load material sets
            5. load models, create material sets from models' information.
        - [x] Add combined image sampler to descriptor pool.
        - [x] implement per-frame vkbufferdeviceaddress for the `Environment_data` as a push-constant (see https://howtovulkan.com/#graphics-pipeline)
            - see also https://howtovulkan.com/#shader-data-buffers
        - [x] Fix renderpass error.
        - [ ] ~~Move the `all_texture_infos` descriptor from shader_basic_diffuse_vulkan.cpp to the actual vulkan engine.~~
            - Defer for later refactoring.


## Detour: add camera controls.

- [x] Implement camera controls.
    - [x] Implement input handles for glfw window.
    - [x] Split out the hdr color/depth image into multiple render views (ichiou).
    - [x] Calc camera matrices.
    - [x] Render-view recreation functionality.
    - [x] Include environmental buffer creation/recreation in.
    - [x] move creation of model transform set buffer to gfx-vulkan.
    - [x] Split out getting render view for the frame and render view from the acquiring swapchain image fence. (define `g.wait_until_can_start_next_frame();`)
    - [x] Upload camera matrices to environmental buffer (before running any shaders).
        - [x] Define `g.set_render_view(cam_matrix.projection, cam_matrix.view);`
        - [x] Set actual environment buffer information.
        - [x] Fix env data buffers not getting created after first frame (render view hdr images were getting in the way).
    - [x] Rename `Render_view_hdr_image` to `Render_view` and `render_view_hdr_images` to `render_views`.
        - Compile did exit 0 👍
    - [x] Update the camera.
    - [x] Figure out why the camera snaps to a certain angle for the first delta.
        - It turns out it was an invalid 0,0 state that was the default from glfw.


## Load materials.

- [x] Include light direction information in environment buffer.
    - Here's how to structure the information (for directional light):
        > vec4 dir.x
        >      dir.y
        >      dir.z
        >      light_intensity
        > uint32_t rgb encoded into 3 bytes, last byte unknown for now.
    - [x] do ichiou
    - [x] Figure out why not working. Padding issue maybe?
        - Seems like padding issue, but just changed the uint32_t to vec4... and that fixed it?
    - [x] It appears that there's an issue with the memory here: [image](20260305_vertex_buffer_capture.png)
        - Turns out that the UV coords were getting written to the normal slot, and also normal directions were getting multiplied by the model matrix (which is 0 rn due to the previous error).

- [x] Get shader params into the basic shader.
    - [x] remove the shader type requirement for the public interface. Only shader name requeired.
    - [x] MISC: do a bunch of cleanup lol
    - [x] build ifc
    - [x] get material params into list
    - [x] move allocated buffer to its own thingy
    - [x] correct assignment of texture idx.
    - [x] trigger build material param collections.
    - [x] create buffer for material param sets
        - [x] create buffer
        - [x] add buffer to the device address list.
    - [x] Import EnTT.
    - [x] Make render object a EnTT component.
    - [ ] add in model transforms for the instances.
        - Perhaps at this point use the render objects?
            - yeah i think it's time.
        - [x] Add sample render object configs.
        - [x] Add/remove render objects from these configs.
        - [x] Write all render objects into the per-instance buffer with the respective transform information too.
            - [x] Think: should transform info be handled from the Render_object_config struct now?
                - I think that interpolation will have to be held onto by the renderer information.
                - Perhaps the information from the ecs registry should only be accessed once per tick frame?
                    - And then the interpolation points would be copied over just once every tick frame.
                > ^^ THIS ^^ is how it should get implemented.
            - [x] Added some of ^^ this ^^ implementation as a TODO.

            - [x] Copy correct transforms over.

            - [x] write model transform buffer.
            - [d] write material param set idx buffer.
                - Deferred: until we know what's oging on w material sets (not to be confused with material param sets).
            
            - [x] Create the per-instance buffer
                - [x] Is model transform buffer created alreayd?
                    - Yes.
        
        - Ok so at this point the lights seem to be working better? Idk.
        - But there needs to be the per-instance data used and model transform used now!

        - [x] Use model transform and material param set idx from the per-instance data in shader now!

        - This works. On the cpu side it'll just have to get better.
        - It is weird tho that it seems to only be refreshing at 30fps or so. idk why that is.
            - imgui metrics say it's running at >500fps (cpu side).
            - But yeah what about gpu side?
            > I put this train of thought into "Performance" section below.


    - [x] create buffer for per-instance data collection
        - [x] create buffer
        - [x] add buffer to the device address list.

- [x] Now material sets have to get created so that the right ones get written to the per-isntance data collection.
    - Maybe rename these to material-palettes? That would ease confusion of the material-set and material-param-set difference.
        > THOUGHT: I think this is a good idea but later. I'll put this in the "Refactor" section below.
    

    - These have to be loaded in by the model or added as a special case hmmm.
        - [x] Created the material collection struct.
            - May need a rename, but it holds shader names and collects material param set names to translate into indexes later during the renderobject creation process for drawing.
        - [x] Use the indexes form the mat-coll struct to create the render objects.
            - Used partial implementation of mat-coll.
        - [x] generate the material and shader indexes while make_material() is called.
        - [x] uses render model indexes when creating render object.

        - [x] Define the funcs.
            - [x] Ehhh for the most part.
            - [x] Just filled the undefined ones with assert-false.

        - [x] Create materials and material palettes func.
            - Turns out only needed material palette creation.

        - [x] Create materal palette ifc.

        - [x] Define emplace_material_palette()

        - [x] Fix .(w)obj files not creating a material palette.


## Creatable render views.

- [x] Move the `build_content()` into its own file.
- [x] Main menubar.
- [x] Placeholder windows for the main viewport and scene editor(s).
- [x] Change the size of main view when main viewport is resized.
- [ ] BUGFIX: why is there such a weird thing going on w/ the image?
    - It seems like it's just drawing onto the same image over and over.
    - This may be solved with the next point "put main view into main viewport."?
- [x] Put main view into main viewport.
    - [x] Put in the groundwork
    - [x] There appears to be no imageview for the main color image.
        - This gets deleted right after the imgui thing happens, so it's getting borked. It's essentially a nullrefexception.
        - I think just putting the old render view into an "old renderviews" queue and then deleting it a frame later would be wiser.
            - Then the old references could exist in the vulkan datastructure for the rest of the frame until things get recreated.
                - @THEA: @SOLUTION: Welp, I tried a different technique (aborting rendering the frame asap when the render view sizes have changed) and that works great! and no double-buffering. There may be stuttering or stuff like that but that should be no problem since the resizing is only occasional.
    - [x] MISC: moved vk-fence checking for after the acquire next image call since that is what informs if the swapchain needs to be recreated. This should be fine tho and make swapchain recreation easier in the future.
    - [x] There's some image transitioning that needs to be done.
    - [ ] ~~Fix swapchain not clearing.~~
        - This might just be solvable with an opaque bg dockspace (see "add dockspace")
    - [x] Fix semaphore validation errors (probs from the rearranging done earlier?)
        - I just revertedd it. It should be fine right? The fence being reset I think should be fine in case something like this happens again.
- [x] Add dockspace.
    - This fixed the clearing visual issue.
- [x] Create more render views for the scene editor views.
- [x] Put scene editor views into the imgui windows.

- [x] have flying camera controls be used on only the window that gets right clicked on.
    - [x] Do actual controls.
    - [x] Get the guide overlay for exiting flying cam mode in there.
        - Partial but it doesn't look good.
        - Now it looks ok decent.
    - [x] Put in functionality of exiting fllying cam mode.
    - [x] Lock cursor.
    - [x] Fix first tick crazy view change issue.


## Small detour: recreate swapchain.

- Since the hdr color image is based off of render-view-sizes vector, it will update correctly.

- [x] Recreate swapchain basically.

- [x] Fix validation errors when resizing.
- [x] Are the old image views and stuff getting properly deleted?!!??!?!
    - maybe but can you check?
    - MoltenVK validation layers says those images are invalid now so I'm assuming the image views are invalid as well too???
    - So image views did have to get deleted. Done'd.


## Refactor.

- [x] Move the `all_texture_infos` descriptor from shader_basic_diffuse_vulkan.cpp to the actual vulkan engine.
    - @NOTE: this must get created before any shaders use the descriptor layout for shader initialization!!!!
- [x] Create functions for things noted to create a function in.
- [x] Rename "material-set" to "material-palette" since there's a bit of a naming confusion between this and "material-param-set" which is the block of params a material needs as input for the shader.
    - [x] Did partially as going thru.
- [x] Rename `struct Material_collection` (generates indexes for material param sets as a shader-local index) and rename `build_material_collection()` (creates GPU buffer for material param sets and uploads data up to them) to something else since these are different things and the current names don't describe what they're doing.
    - [x] renaming to `struct Material_organizer`
    - [x] renaming to `organize_materials()`


## Get actual render objects working with multiple models and materials.

- [x] Have render objects' model info be used when drawing with shader stuff.
    - i.e. replace the `k_cmd_draw_fn` lambda in the diffuse shader.
    - [x] created a new renderer setup and interfaces for the right data set up for `set_render_object_per_instance_data()`.
- [x] Put render objects' sub-meshes into buckets (one for each shader) so that it can render stuff.
- [x] Have shaders render what's in their buckets.
    - Or just skip if there's nothing in their buckets!
- [x] Rework the #if0'd code in `set_render_object_per_instance_data()`
    - Make sure that the right number of instances and what to put for instances is done.


## Can quit out.

- [x] let's see whats needed.
- [x] all done!


## Getting the orbit cam stuff going.

- Basically just having the orbit cam be done while it's simulation mode.
- Have a "toggle locking camera into main viewport with shift+C" while during the simulation mode.

- When simulatino starts just start doing orbit camera.

- [x] add toggle shift+c prompt and actions.
    - A bit buggy but good enough for now

- [x] Change the cam type for idx=0 to orbit cam instead.

- [x] Implement the stubbed orbit cam funcs.


## Implementing animation and AFA.

- [x] Load in .btafa and .btanitor information.
    - This appears to be the way that it gets loaded in (.btanitor ohh .btafa as well!):
        ```cpp
        // Create render object.
        Render_object new_rend_obj{ rend_obj_settings.render_layer };

        auto const& model{ *Model_bank::get_model(rend_obj_settings.model_name) };
        if (allow_deformed_creation && rend_obj_settings.is_deformed)
        {   // Create deformed model w/ animator.
            auto deformed_model{ std::make_unique<Deformed_model>(model) };

            bool has_root_motion_tag{ reg.any_of<component::Animator_root_motion>(entity) };
            auto model_animator{ std::make_unique<Model_animator>(model, has_root_motion_tag) };

            service_finder::find_service<Animator_template_bank>()
                .load_animator_template_into_animator(*model_animator,
                                                      rend_obj_settings.animator_template_name);    // @HERE

            new_rend_obj.set_deformed_model(std::move(deformed_model));
            new_rend_obj.set_model_animator(std::move(model_animator));

            // Set first anim as default state-set.
            assert(new_rend_obj.get_model_animator()->get_animator_states().size() >= 1);
            new_rend_obj.get_model_animator()->change_state_set(
                { .anim_state_indices = { 0 }, .loop_final_state = true });

            // Check for anim frame action controller configuration.
            auto afa_ctrller{ reg.try_get<component::Anim_frame_action_controller>(entity) };
            if (afa_ctrller != nullptr)
            {   // Configure anim frame action data.
                auto const& afa_ctrller_ref{ anim_frame_action::Bank::get(                          // @HERE
                    afa_ctrller->anim_frame_action_controller_name) };

                new_rend_obj.get_model_animator()->configure_anim_frame_action_controls(            // @HERE
                    &afa_ctrller_ref,
                    entity_container.find_entity_uuid(entity),
                    Model_animator::make_jump_queue_create_list_from_anim_frame_action_controls(
                        afa_ctrller_ref));

                // Add hitcapsule set driver.
                reg.emplace_or_replace<component::Animator_driven_hitcapsule_set>(entity);
            }
        }
        else
        {   // Use static model from bank.
            new_rend_obj.set_model(Model_bank::get_model(rend_obj_settings.model_name));
        }

        UUID rend_obj_uuid{ rend_obj_pool.emplace(std::move(new_rend_obj)) };

        // Attach render object id as new component.
        reg.emplace<component::Created_render_object_reference>(entity, rend_obj_uuid);

        BT_TRACEF("Created and emplaced \"%s\" into render object pool.",
                  UUID_helper::to_pretty_repr(rend_obj_uuid).c_str());
        ```


- [x] Create the deformed animation copy.
    - [x] use the static model for per-instance placement.
    - [x] Allocate space for vertices of the deformed mesh.
        - Maybe for now just do a copy?
        - Aaaaa do i wanna share the same buffer for the deformed meshes and static ones?
            - Nope nop enop. it'll be a lot of copying just for this.
            - combining the deformed meshes might work tho? mmmm it would mean that there would be stuttering for when deformed models are added and removed.
                - eh ig it would just be best if that just happened since making 2 buffers would suck.
        - Ok so i decided on combined model and it just seems like it's just rly similar to `upload_model_entries_to_gpu()`.
    - [x] Use deformed model instead of static model.
    - [x] CHECK: that the deformed model is being used.
        - Pretty sure it is if the bindings are changing.

- [x] Write compute shader for the animation info.
    - [x] Make initial implementation of the compute shader.
    - [x] Create data structures/ifc for accessing needed model skin and joint trnas buffers.
    - [x] Create skin data buffer from skin data after loaded from gltf.
        - [x] marked
    - [x] get the model skin reference assigned to deformed model.
    - [x] Create joint transforms buffer.
        - [x] marked
        - Should use "is_created()" in vk_buffer::allocated_buffer.
    - [x] Create descriptor set for the combined deformed vertex set (and keep it updated whenever that combined model changes or gets rebuilt).
        - [x] marked
        - [x] stubbed in shader class
    - [x] Update joint transforms buffer with animator stuff while it's playing.
        - [x] marked
        - [x] kinda stubbed out w pseudocode
    - [x] Actually call the shader for each deformed model.
        - [x] marked
        - [x] kinda stubbed out w pseudocode
    - [x] fix compiler errors
    - [x] It looks like that check for the allocated buffer fails for render view buffers. Should fix that eh!
    - [x] fix memory corruption of model skin when using model animations

- [x] fixes dangling vulkan buffers.
- [x] fixes buffer missing deletion check false-positive when resizing render view list.
    - just disables the check right when doing the resizing (for only the elems that aren't getting deleted).


## Performance.

- [ ] For some reason the window seems to be running at 30fps or so on gpu, but the imgui metrics say ">500fps" when measuring on the cpu side. What's going on?