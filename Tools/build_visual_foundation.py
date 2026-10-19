"""Visual worktree only: non-colliding cockpit art and presentational map changes."""
from pathlib import Path
import json,unreal
root=Path(__file__).resolve().parents[1]
assert root.name=='Road-To-F1-Visual','Run only in the visual worktree'
DEST='/Game/RoadToF1/Art/VisualV01'; tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/RoadToF1/SouthGarda_KartRace')
def vec(v):return [v.x,v.y,v.z]
def transform(t):return {'position':vec(t.translation),'rotation':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],'scale':vec(t.scale3d)}
def fingerprint():
 result={}
 for a in actors.get_all_level_actors():
  if isinstance(a,unreal.StaticMeshActor) or a.get_class().get_name() in ['RaceTrackGate','KartRaceDirector','PlayerStart']:
   data={'class':a.get_class().get_name(),'transform':transform(a.get_actor_transform())}
   if isinstance(a,unreal.StaticMeshActor):
    c=a.static_mesh_component;data.update(mesh=c.static_mesh.get_path_name() if c.static_mesh else '',collision=str(c.get_collision_profile_name()))
   if a.get_class().get_name()=='RaceTrackGate':data['order']=a.get_editor_property('order')
   if a.get_class().get_name()=='KartRaceDirector':data.update(route=[vec(v) for v in a.get_editor_property('route_points')],grid=[transform(t) for t in a.get_editor_property('grid')])
   result[a.get_name()]=data
 return result
before=fingerprint()
def import_asset(file,dest,name,mesh=False):
 task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=dest;task.destination_name=name
 task.automated=task.save=task.replace_existing=True
 if mesh:
  ui=unreal.FbxImportUI();ui.automated_import_should_detect_type=False;ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
  ui.import_mesh=True;ui.import_materials=ui.import_textures=False
  ui.static_mesh_import_data.combine_meshes=True;ui.static_mesh_import_data.auto_generate_collision=False;ui.static_mesh_import_data.convert_scene=False;task.options=ui
 tools.import_asset_tasks([task]);asset=unreal.load_asset(dest+'/'+name);assert asset,'Import failed: '+name;return asset
manifest=json.loads((root/'Art/VisualOverhaul/exports/manifest.json').read_text())
for entry in manifest['kart']+manifest['wheel']:
 sm=import_asset(root/'Art/VisualOverhaul/exports'/entry['file'],DEST+'/Meshes',Path(entry['file']).stem,True)
 mat=unreal.load_asset('/Game/RoadToF1/Art/SouthGardaV01/Materials/M_'+entry['material']);assert mat
 for i in range(len(sm.get_editor_property('static_materials'))):sm.set_material(i,mat)
 unreal.EditorAssetLibrary.save_loaded_asset(sm)
def linear(v):return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
def material(name,color,roughness,texture=None,tile=300):
 path=DEST+'/Materials/M_'+name
 mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_'+name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
 lib.delete_all_material_expressions(mat)
 if texture:
  tex=import_asset(root/'Art/VisualOverhaul/Textures'/texture,DEST+'/Textures',Path(texture).stem)
  pos=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-600,0)
  mask=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask,-450,0);mask.set_editor_property('r',True);mask.set_editor_property('g',True);mask.set_editor_property('b',False);mask.set_editor_property('a',False)
  mul=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply,-300,0);mul.set_editor_property('const_b',1/tile)
  sample=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-100,0);sample.texture=tex
  lib.connect_material_expressions(pos,'',mask,'Input');lib.connect_material_expressions(mask,'',mul,'A');lib.connect_material_expressions(mul,'',sample,'Coordinates')
  lib.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
 else:
  c=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-200,0);c.constant=unreal.LinearColor(*(linear(v/255) for v in color),1)
  lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
 rough=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-200,200);rough.r=roughness
 lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS);lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);return mat
mats={'Circuit_Asphalt':material('Asphalt',(0,0,0),.88,'Asphalt_Variation.png',300),
 'Circuit_Grass':material('Grass',(0,0,0),.97,'Grass_Variation.png',1600),
 'Circuit_Kerb_blue':material('KerbBlue',(30,132,190),.65),
 'Circuit_Kerb_blue_surface':None,'Circuit_Kerb_white':material('KerbWhite',(238,241,226),.72),
 'Circuit_RoadPaint':material('RoadPaint',(240,239,225),.78)}
mats['Circuit_Kerb_blue_surface']=mats['Circuit_Kerb_blue']
inspection=[]
for a in actors.get_all_level_actors():
 inspection.append({'label':a.get_actor_label(),'class':a.get_class().get_name()})
 if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh:
  name=a.static_mesh_component.static_mesh.get_name()
  if name in mats:
   for i in range(a.static_mesh_component.get_num_materials()):a.static_mesh_component.set_material(i,mats[name])
  if name.startswith('Circuit_Kerb'):
   tags=list(a.tags)
   if 'VisualKerb' not in [str(t) for t in tags]:tags.append('VisualKerb')
   a.tags=tags
 if isinstance(a,unreal.DirectionalLight):
  a.set_actor_rotation(unreal.Rotator(pitch=-38,yaw=-35,roll=0),False)
  a.light_component.set_editor_property('intensity',45000)
  a.light_component.set_editor_property('light_color',unreal.Color(255,244,229,255))
  a.light_component.set_editor_property('light_source_angle',1.0)
 if isinstance(a,unreal.SkyLight):
  a.light_component.set_editor_property('intensity',1.0)
  a.light_component.set_editor_property('real_time_capture',True)
 if isinstance(a,unreal.ExponentialHeightFog):
  f=a.get_component_by_class(unreal.ExponentialHeightFogComponent)
  f.set_editor_property('fog_density',.008);f.set_editor_property('fog_height_falloff',.2)
  f.set_editor_property('start_distance',6000);f.set_editor_property('fog_max_opacity',.18)
  f.set_editor_property('enable_volumetric_fog',False)
post=next((a for a in actors.get_all_level_actors() if a.get_actor_label()=='Visual_Foundation_PostProcess'),None)
if not post:post=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
post.set_actor_label('Visual_Foundation_PostProcess');post.set_editor_property('unbound',True)
settings=post.get_editor_property('settings')
values={'auto_exposure_min_brightness':12.5,'auto_exposure_max_brightness':12.5,'auto_exposure_bias':0.0,
 'bloom_intensity':.12,'motion_blur_amount':0.0,'scene_fringe_intensity':0.0,'film_grain_intensity':0.0,
 'color_saturation':unreal.Vector4(1.04,1.04,1.04,1),'color_contrast':unreal.Vector4(1.04,1.04,1.04,1)}
for key,value in values.items():settings.set_editor_property('override_'+key,True);settings.set_editor_property(key,value)
post.set_editor_property('settings',settings)
after=fingerprint()
(root/'TestResults/VisualOverhaul/geometry-check.json').write_text(json.dumps({'before':before,'after':after},indent=2))
assert before==after,'Gameplay geometry or race configuration changed'
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/RoadToF1/SouthGarda_KartRace')
report={'geometry_and_race_configuration_unchanged':True,'before':before,'after':after,'original_actors':inspection,'post_process':{k:str(v) for k,v in values.items()},'sun_lux':45000,'plugins_persistently_enabled':[]}
(root/'TestResults/VisualOverhaul/asset-report.json').write_text(json.dumps(report,indent=2))
unreal.log('VISUAL_FOUNDATION_IMPORT_OK')
