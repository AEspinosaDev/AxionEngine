// class Mesh{
//     std::vector<floats> vertices;
//     std::vector<uint> indices;
//     std::map<stride, format> attributes;

// }

// struct MeshHandle{
//     uint handle;
// }

// class AssetManager{


//     std::vector<Mesh> meshes;
//     std::vector<Material> materials;


//     MeshHandle load(loading from  GLTF/PLY7OBJ)
//     MeshHandle createQuad
//     MeshHandle createTri
//     MeshHandle createSpheree
//     MeshHandle createCube

// }

// Scene deberia tener aparte de la esteuctrura de datros aplanada registry,
//  una estuctura de arbol comn children y a su bvez los entities deberian
//  tener hijos y un padre. Eso se usara en el momento adecuadop para calcular su matriz model teniendo en cuenta los padres

//  core::Renderer{
//     Graphics::Renderer* _rnd;
//     //Handles a resoruces basicos GPU
//     Per frame UBOs
//     Dummy and fallback Textures
//     Samplers
//     Global Vertex and Index Buffers for vertex pulling all geometries
//     A list of strings storing paths to shaders and pipelines (podria ser user input pero por ahora simple)

//     la funcion mas importante es 
//     void render( Core::Scene::Scene& scene ){
//         dentro o en algun lugar, debe existir una GPUScene, esta gpuScene tiene GPUMeshes, que no son mas que contendores que juntan
//         el handle CPU (Asset::Mesh) cons los handles GPU (bufferVertexOffeset y textureHandles etc)
//         Algo como  
//         struct GPUMesh{
//             Asset::MeshHandle handle 
//             uint offsetBuffer
//             uint offsetIndexBuffer
//             (Si fuera raster clasico aqui habria un IBOHandle y un VBOHandle pero como hacermos vetex pullin)
//         }
// Esta gpu scene deberia guardar una cache de GPUMeshes, cadaa vez que una nueva entra se preparan los handles y se le indica al renderer interno que haga una pasad de upload
// la cache de GPUMeshes podria esta indezada por el EntityID???? Nose 
// Tampoco tengo claro si deberia GPUScene habitar fuera o dentro de core::renderer


//     }
// Este core renderer tendria metodos internos para preparar uniforms de los objetos y subirlos a sus respectivos ubos y etc.
// Por ahora no quiero complpicarme con materiales. Me gustaria implementar todo este sistema simpkemente con mayas y una pasada raster con un solo shader por ahora, 
// pero tener resuelto todo el tema de gestion de meshes. 
//  }

//  Como ves la idea mas o menos? Para empezar a trabajar habira que definir el AssetManager, que como ves funciona parecido a un ECS o a 
//  los GPUResourcePool que ya tengo. Crear una funcion de loadGLTF o loadOBJ par ya por fin cargar mayas normales y too lo demas
//  Como ves el plan? que fallas tiene, o lo ves bien