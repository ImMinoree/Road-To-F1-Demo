from pathlib import Path
import random,math
from PIL import Image
root=Path(__file__).resolve().parent;(root/'Textures').mkdir(exist_ok=True)
rng=random.Random(58)
# Tileable analytic low-frequency variation plus subtle seeded surface grain.
for name,base,grain in [('Asphalt',(61,69,76),5),('Grass',(77,120,49),2)]:
 im=Image.new('RGB',(512,512));px=im.load()
 for y in range(512):
  for x in range(512):
   v=math.sin(x/512*math.tau*3+math.sin(y/512*math.tau*2))*.025+math.cos((x+y)/512*math.tau*5)*.018
   noise=rng.uniform(-grain,grain)
   px[x,y]=tuple(max(0,min(255,round(c*(1+v)+noise))) for c in base)
 im.save(root/'Textures'/f'{name}_Variation.png')

