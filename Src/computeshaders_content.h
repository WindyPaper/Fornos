// Auto-generated file with shaders2cpp.py utility

const char ao_step0_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\nstruct Output\n{\nvec3 o;\nvec3 d;\nvec3 tx;\nvec3 ty;\n};\nlayout(location = 1) uniform uint pixOffset;\nlayout(std430, binding = 2) readonly buffer meshPBuffer { vec3 positions[]; };\nlayout(std430, binding = 3) readonly buffer meshNBuffer { vec3 normals[]; };\nlayout(std430, binding = 4) readonly buffer coordsBuffer { vec4 coords[]; };\nlayout(std430, binding = 5) readonly buffer coordsTidxBuffer { uint coords_tidx[]; };\nlayout(std430, binding = 6) writeonly buffer outputBuffer { Output outputs[]; };\n \nvec3 getPosition(uint tidx, vec3 bcoord)\n{\nvec3 p0 = positions[tidx + 0];\nvec3 p1 = positions[tidx + 1];\nvec3 p2 = positions[tidx + 2];\nreturn bcoord.x * p0 + bcoord.y * p1 + bcoord.z * p2;\n}\nvec3 getNormal(uint tidx, vec3 bcoord)\n{\nvec3 n0 = normals[tidx + 0];\nvec3 n1 = normals[tidx + 1];\nvec3 n2 = normals[tidx + 2];\nreturn normalize(bcoord.x * n0 + bcoord.y * n1 + bcoord.z * n2);\n}\nvoid main()\n{\nuint in_idx = gl_GlobalInvocationID.x + pixOffset;\nuint out_idx = gl_GlobalInvocationID.x;\nvec4 coord = coords[in_idx];\nuint tidx = coords_tidx[in_idx];\nvec3 o = getPosition(tidx, coord.yzw);\nvec3 d = getNormal(tidx, coord.yzw);\nvec3 ty = normalize(abs(d.x) > abs(d.y) ? vec3(d.z, 0, -d.x) : vec3(0, d.z, -d.y));\nvec3 tx = cross(d, ty);\noutputs[out_idx].o = o;\noutputs[out_idx].d = d;\noutputs[out_idx].tx = tx;\noutputs[out_idx].ty = ty;\n}\n";
const char ao_step1_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\n#define FLT_MAX 3.402823466e+38\nstruct Params\n{\nuint sampleCount;  \nuint samplePermCount;\nfloat minDistance;\nfloat maxDistance;\n};\nstruct BVH\n{\nfloat aabbMinX; float aabbMinY; float aabbMinZ;\nfloat aabbMaxX; float aabbMaxY; float aabbMaxZ;\nuint start;\nuint end;\nuint jump;  \n};\nstruct Input\n{\nvec3 o;\nvec3 d;\nvec3 tx;\nvec3 ty;\n};\nlayout(location = 1) uniform uint pixOffset;\nlayout(location = 2) uniform uint bvhCount;\nlayout(std430, binding = 3) readonly buffer paramsBuffer { Params params; };\nlayout(std430, binding = 4) readonly buffer meshPBuffer { vec3 positions[]; };\nlayout(std430, binding = 5) readonly buffer bvhBuffer { BVH bvhs[]; };\nlayout(std430, binding = 6) readonly buffer samplesBuffer { vec3 samples[]; };\nlayout(std430, binding = 7) readonly buffer inputsBuffer { Input inputs[]; };\nlayout(std430, binding = 8) writeonly buffer resultAccBuffer { float results[]; };\nuniform sampler2D diffuse_tex;\nfloat RayAABB(vec3 o, vec3 d, vec3 mins, vec3 maxs)\n{\nvec3 t1 = (mins - o) / d;\nvec3 t2 = (maxs - o) / d;\nvec3 tmin = min(t1, t2);\nvec3 tmax = max(t1, t2);\nfloat a = max(tmin.x, max(tmin.y, tmin.z));\nfloat b = min(tmax.x, min(tmax.y, tmax.z));\nreturn (b >= 0 && a <= b) ? a : FLT_MAX;\n}\nvec3 barycentric(vec3 p, vec3 a, vec3 b, vec3 c)\n{\nvec3 v0 = b - a;\nvec3 v1 = c - a;\nvec3 v2 = p - a;\nfloat d00 = dot(v0, v0);\nfloat d01 = dot(v0, v1);\nfloat d11 = dot(v1, v1);\nfloat d20 = dot(v2, v0);\nfloat d21 = dot(v2, v1);\nfloat denom = d00 * d11 - d01 * d01;\nfloat y = (d11 * d20 - d01 * d21) / denom;\nfloat z = (d00 * d21 - d01 * d20) / denom;\nreturn vec3(1.0 - y - z, y, z);\n}\n \nfloat raycast(vec3 o, vec3 d, vec3 a, vec3 b, vec3 c)\n{\nvec3 n = normalize(cross(b - a, c - a));\nfloat nd = dot(d, n);\nif (abs(nd) > 0)\n{\nfloat pn = dot(o, n);\nfloat t = (dot(a, n) - pn) / nd;\nif (t >= 0)\n{\nvec3 p = o + d * t;\nvec3 b = barycentric(p, a, b, c);\nif (b.x >= 0 &&  \nb.y >= 0 && b.y <= 1 &&\nb.z >= 0 && b.z <= 1)\n{\nreturn t;\n}\n}\n}\nreturn FLT_MAX;\n}\nfloat raycastRange(vec3 o, vec3 d, uint start, uint end, float mindist)\n{\nfloat mint = FLT_MAX;\nfor (uint tidx = start; tidx < end; tidx += 3)\n{\nvec3 v0 = positions[tidx + 0];\nvec3 v1 = positions[tidx + 1];\nvec3 v2 = positions[tidx + 2];\nfloat t = raycast(o, d, v0, v1, v2);\nif (t >= mindist && t < mint)\n{\nmint = t;\n}\n}\nreturn mint;\n}\nfloat raycastBVH(vec3 o, vec3 d, float mindist, float maxdist)\n{\nfloat mint = FLT_MAX;\nuint i = 0;\nwhile (i < bvhCount)\n{\nBVH bvh = bvhs[i];\nvec3 aabbMin = vec3(bvh.aabbMinX, bvh.aabbMinY, bvh.aabbMinZ);\nvec3 aabbMax = vec3(bvh.aabbMaxX, bvh.aabbMaxY, bvh.aabbMaxZ);\nfloat distAABB = RayAABB(o, d, aabbMin, aabbMax);\nif (distAABB < mint && distAABB < maxdist)\n \n{\nfloat t = raycastRange(o, d, bvh.start, bvh.end, mindist);\nif (t < mint)\n{\nmint = t;\n}\n++i;\n}\nelse\n{\ni = bvh.jump;\n}\n}\nreturn mint;\n}\nvoid main()\n{\nuint in_idx = gl_GlobalInvocationID.x / params.sampleCount;\nuint pix_idx = in_idx + pixOffset;\nuint sample_idx = gl_GlobalInvocationID.x % params.sampleCount;\nuint out_idx = gl_GlobalInvocationID.x;\nInput idata = inputs[in_idx];\nvec3 o = idata.o;\nvec3 d = idata.d;\nvec3 tx = idata.tx;\nvec3 ty = idata.ty;\nuint sidx = (pix_idx % params.samplePermCount) * params.sampleCount + sample_idx;\nvec3 rs = samples[sidx];\nvec3 sampleDir = normalize(tx * rs.x + ty * rs.y + d * rs.z);\nfloat t = raycastBVH(o, sampleDir, params.minDistance, params.maxDistance);\nif (t != FLT_MAX && t < params.maxDistance)\n{\nresults[out_idx] = 1;\n}\nelse\n{\nresults[out_idx] = 0;\n}\n}\n";
const char ao_step2_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\nstruct Params\n{\nuint sampleCount;  \nfloat minDistance;\nfloat maxDistance;\n};\nlayout(location = 1) uniform uint workOffset;\nlayout(std430, binding = 2) readonly buffer paramsBuffer { Params params; };\nlayout(std430, binding = 3) readonly buffer dataBuffer { float data[]; };\nlayout(std430, binding = 4) writeonly buffer resultAccBuffer { float results[]; };\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x;\nuint data_start_idx = gid * params.sampleCount;\nfloat acc = 0;\nfor (uint i = 0; i < params.sampleCount; ++i)\n{\nacc += data[data_start_idx + i];\n}\nuint result_idx = gid + workOffset;\nresults[result_idx] = 1.0 - acc / float(params.sampleCount);\n}\n";
const char bentnormals_step1_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\n#define BUFFER_PARAMS 3\n#define BUFFER_POSITIONS 12\n#define BUFFER_BVH 8\n#define BUFFER_SAMPLES 13\n#define BUFFER_RESULTS_ACC 11\n#define BUFFER_INPUTS 14\n#define FLT_MAX 3.402823466e+38\nstruct Params\n{\nuint sampleCount;  \nuint samplePermCount;\nfloat minDistance;\nfloat maxDistance;\n};\nstruct BVH\n{\nfloat aabbMinX; float aabbMinY; float aabbMinZ;\nfloat aabbMaxX; float aabbMaxY; float aabbMaxZ;\nuint start;\nuint end;\nuint jump;  \n};\nstruct Input\n{\nvec3 o;\nvec3 d;\nvec3 tx;\nvec3 ty;\n};\nlayout(location = 1) uniform uint pixOffset;\nlayout(location = 2) uniform uint bvhCount;\nlayout(std430, binding = 3) readonly buffer paramsBuffer { Params params; };\nlayout(std430, binding = 4) readonly buffer meshPBuffer { vec3 positions[]; };\nlayout(std430, binding = 5) readonly buffer bvhBuffer { BVH bvhs[]; };\nlayout(std430, binding = 6) readonly buffer samplesBuffer { vec3 samples[]; };\nlayout(std430, binding = 7) readonly buffer inputsBuffer { Input inputs[]; };\nlayout(std430, binding = 8) writeonly buffer resultAccBuffer { vec3 results[]; };\nfloat RayAABB(vec3 o, vec3 d, vec3 mins, vec3 maxs)\n{\n \n \n \n \nvec3 t1 = (mins - o) / d;\nvec3 t2 = (maxs - o) / d;\nvec3 tmin = min(t1, t2);\nvec3 tmax = max(t1, t2);\nfloat a = max(tmin.x, max(tmin.y, tmin.z));\nfloat b = min(tmax.x, min(tmax.y, tmax.z));\nreturn (b >= 0 && a <= b) ? a : FLT_MAX;\n}\nvec3 barycentric(vec3 p, vec3 a, vec3 b, vec3 c)\n{\nvec3 v0 = b - a;\nvec3 v1 = c - a;\nvec3 v2 = p - a;\nfloat d00 = dot(v0, v0);\nfloat d01 = dot(v0, v1);\nfloat d11 = dot(v1, v1);\nfloat d20 = dot(v2, v0);\nfloat d21 = dot(v2, v1);\nfloat denom = d00 * d11 - d01 * d01;\n \nfloat y = (d11 * d20 - d01 * d21) / denom;\nfloat z = (d00 * d21 - d01 * d20) / denom;\nreturn vec3(1.0 - y - z, y, z);\n}\n \nfloat raycast(vec3 o, vec3 d, vec3 a, vec3 b, vec3 c)\n{\nvec3 n = normalize(cross(b - a, c - a));\nfloat nd = dot(d, n);\nif (abs(nd) > 0)\n{\nfloat pn = dot(o, n);\nfloat t = (dot(a, n) - pn) / nd;\nif (t >= 0)\n{\nvec3 p = o + d * t;\nvec3 b = barycentric(p, a, b, c);\nif (b.x >= 0 &&  \nb.y >= 0 && b.y <= 1 &&\nb.z >= 0 && b.z <= 1)\n{\nreturn t;\n}\n}\n}\nreturn FLT_MAX;\n}\nfloat raycastRange(vec3 o, vec3 d, uint start, uint end, float mindist)\n{\nfloat mint = FLT_MAX;\nfor (uint tidx = start; tidx < end; tidx += 3)\n{\nvec3 v0 = positions[tidx + 0];\nvec3 v1 = positions[tidx + 1];\nvec3 v2 = positions[tidx + 2];\nfloat t = raycast(o, d, v0, v1, v2);\nif (t >= mindist && t < mint)\n{\nmint = t;\n}\n}\nreturn mint;\n}\nfloat raycastBVH(vec3 o, vec3 d, float mindist, float maxdist)\n{\nfloat mint = FLT_MAX;\nuint i = 0;\nwhile (i < bvhCount)\n{\nBVH bvh = bvhs[i];\nvec3 aabbMin = vec3(bvh.aabbMinX, bvh.aabbMinY, bvh.aabbMinZ);\nvec3 aabbMax = vec3(bvh.aabbMaxX, bvh.aabbMaxY, bvh.aabbMaxZ);\nfloat distAABB = RayAABB(o, d, aabbMin, aabbMax);\nif (distAABB < mint && distAABB < maxdist)\n \n{\nfloat t = raycastRange(o, d, bvh.start, bvh.end, mindist);\nif (t < mint)\n{\nmint = t;\n}\n++i;\n}\nelse\n{\ni = bvh.jump;\n}\n}\nreturn mint;\n}\nvoid main()\n{\nuint in_idx = gl_GlobalInvocationID.x / params.sampleCount;\nuint pix_idx = in_idx + pixOffset;\nuint sample_idx = gl_GlobalInvocationID.x % params.sampleCount;\nuint out_idx = gl_GlobalInvocationID.x;\nInput idata = inputs[in_idx];\nvec3 o = idata.o;\nvec3 d = idata.d;\nvec3 tx = idata.tx;\nvec3 ty = idata.ty;\nuint sidx = (pix_idx % params.samplePermCount) * params.sampleCount + sample_idx;\nvec3 rs = samples[sidx];\nvec3 sampleDir = normalize(tx * rs.x + ty * rs.y + d * rs.z);\nfloat t = raycastBVH(o, sampleDir, params.minDistance, params.maxDistance);\nresults[out_idx] = (t != FLT_MAX) ? vec3(0,0,0) : sampleDir;\n}\n";
const char bentnormals_step2_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\nstruct Params\n{\nuint sampleCount;  \nfloat minDistance;\nfloat maxDistance;\n};\nstruct V3 { float x; float y; float z; };\nlayout(location = 1) uniform uint workOffset;\nlayout(std430, binding = 2) readonly buffer paramsBuffer { Params params; };\nlayout(std430, binding = 3) readonly buffer dataBuffer { vec3 data[]; };\nlayout(std430, binding = 4) writeonly buffer resultAccBuffer { V3 results[]; };\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x;\nuint data_start_idx = gid * params.sampleCount;\nvec3 acc = vec3(0, 0, 0);\nfor (uint i = 0; i < params.sampleCount; ++i)\n{\nacc += data[data_start_idx + i];\n}\nvec3 normal = normalize(acc);\nuint result_idx = gid + workOffset;\nresults[result_idx].x = normal.x;\nresults[result_idx].y = normal.y;\nresults[result_idx].z = normal.z;\n}\n";
const char heights_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\n#define FLT_MAX 3.402823466e+38\nlayout(location = 1) uniform uint workOffset;\nlayout(std430, binding = 2) readonly buffer coordsBuffer { vec4 coords[]; };\nlayout(std430, binding = 3) writeonly buffer resultBuffer { float results[]; };\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x + workOffset;\nvec4 coord = coords[gid];\nfloat height = coord.x;\nresults[gid] = height != FLT_MAX ? height : 0;\n}\n";
const char meshmapping_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\n#define RAYCAST_FORWARD 1\n#define RAYCAST_BACKWARD 1\n#define FLT_MAX 3.402823466e+38\n#define BARY_MIN -1e-5\n#define BARY_MAX 1.0\nstruct Pix\n{\nvec3 p;\nvec3 d;\n};\nstruct BVH\n{\nfloat aabbMinX; float aabbMinY; float aabbMinZ;\nfloat aabbMaxX; float aabbMaxY; float aabbMaxZ;\nuint start;\nuint end;\nuint jump;  \n};\nlayout(location = 1) uniform uint workOffset;\nlayout(location = 2) uniform uint workCount;\nlayout(location = 3) uniform uint bvhCount;\nlayout(std430, binding = 4) readonly buffer pixBuffer { Pix pixels[]; };\nlayout(std430, binding = 5) readonly buffer meshPBuffer { vec3 positions[]; };\nlayout(std430, binding = 6) readonly buffer bvhBuffer { BVH bvhs[]; };\nlayout(std430, binding = 7) writeonly buffer rCoordBuffer { vec4 r_coords[]; };\nlayout(std430, binding = 8) writeonly buffer rTidxBuffer { uint r_tidx[]; };\nfloat RayAABB(vec3 o, vec3 d, vec3 mins, vec3 maxs)\n{\nvec3 dabs = abs(d);\nvec3 t1 = (mins - o) / d;\nvec3 t2 = (maxs - o) / d;\nvec3 tmin = min(t1, t2);\nvec3 tmax = max(t1, t2);\nfloat a = max(tmin.x, max(tmin.y, tmin.z));\nfloat b = min(tmax.x, min(tmax.y, tmax.z));\nreturn (b >= 0 && a <= b) ? a : FLT_MAX;\n}\nvec3 barycentric(dvec3 p, dvec3 a, dvec3 b, dvec3 c)\n{\ndvec3 v0 = b - a;\ndvec3 v1 = c - a;\ndvec3 v2 = p - a;\ndouble d00 = dot(v0, v0);\ndouble d01 = dot(v0, v1);\ndouble d11 = dot(v1, v1);\ndouble d20 = dot(v2, v0);\ndouble d21 = dot(v2, v1);\ndouble denom = d00 * d11 - d01 * d01;\ndouble y = (d11 * d20 - d01 * d21) / denom;\ndouble z = (d00 * d21 - d01 * d20) / denom;\nreturn vec3(dvec3(1.0 - y - z, y, z));\n}\n \nvec4 raycast(vec3 o, vec3 d, vec3 a, vec3 b, vec3 c)\n{\nvec3 n = normalize(cross(b - a, c - a));\nfloat nd = dot(d, n);\nif (abs(nd) > 0)\n{\nfloat pn = dot(o, n);\nfloat t = (dot(a, n) - pn) / nd;\nif (t >= 0)\n{\nvec3 p = o + d * t;\nvec3 b = barycentric(p, a, b, c);\nif (b.x >= BARY_MIN && b.y >= BARY_MIN && b.y <= BARY_MAX && b.z >= BARY_MIN && b.z <= BARY_MAX)\n{\nreturn vec4(t, b.x, b.y, b.z);\n}\n}\n}\nreturn vec4(FLT_MAX, 0, 0, 0);\n}\nfloat raycastRange(vec3 o, vec3 d, uint start, uint end, float mindist, out uint o_idx, out vec3 o_bcoord)\n{\nfloat mint = FLT_MAX;\nfor (uint tidx = start; tidx < end; tidx += 3)\n{\nvec3 v0 = positions[tidx + 0];\nvec3 v1 = positions[tidx + 1];\nvec3 v2 = positions[tidx + 2];\nvec4 r = raycast(o, d, v0, v1, v2);\nif (r.x >= mindist && r.x < mint)\n{\nmint = r.x;\no_idx = tidx;\no_bcoord = r.yzw;\n}\n}\nreturn mint;\n}\nfloat raycastBVH(vec3 o, vec3 d, float mint, in out uint o_idx, in out vec3 o_bcoord)\n{\nuint i = 0;\nwhile (i < bvhCount)\n{\nBVH bvh = bvhs[i];\nvec3 aabbMin = vec3(bvh.aabbMinX, bvh.aabbMinY, bvh.aabbMinZ);\nvec3 aabbMax = vec3(bvh.aabbMaxX, bvh.aabbMaxY, bvh.aabbMaxZ);\nfloat distAABB = RayAABB(o, d, aabbMin, aabbMax);\nif (distAABB < mint)\n \n{\nuint ridx = 0;\nvec3 rbcoord = vec3(0, 0, 0);\nfloat t = raycastRange(o, d, bvh.start, bvh.end, 0, ridx, rbcoord);\nif (t < mint)\n{\nmint = t;\no_idx = ridx;\no_bcoord = rbcoord;\n}\n++i;\n}\nelse\n{\ni = bvh.jump;\n}\n}\nreturn mint;\n}\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x + workOffset;\nif (gid >= workCount) return;\nPix pix = pixels[gid];\nvec3 p = pix.p;\nvec3 d = pix.d;\nuint tidx = 4294967295;\nvec3 bcoord = vec3(0, 0, 0);\nfloat t = FLT_MAX;\n#if RAYCAST_FORWARD\nt = min(t, raycastBVH(p, d, t, tidx, bcoord));\n#endif\n#if RAYCAST_BACKWARD\nt = min(t, raycastBVH(p, -d, t, tidx, bcoord));\n#endif\nr_coords[gid] = vec4(t, bcoord.x, bcoord.y, bcoord.z);\nr_tidx[gid] = tidx;\n}\n";
const char meshmapping_nobackfaces_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\n#extension GL_ARB_gpu_shader_fp64 : enable\nlayout (local_size_x = 64) in;\n#define FLT_MAX 3.402823466e+38\n#define BARY_MIN -1e-5\n#define BARY_MAX 1.0\nstruct Pix\n{\nvec3 p;\nvec3 d;\n};\nstruct BVH\n{\nfloat aabbMinX; float aabbMinY; float aabbMinZ;\nfloat aabbMaxX; float aabbMaxY; float aabbMaxZ;\nuint start;\nuint end;\nuint jump;  \n};\nlayout(location = 1) uniform uint workOffset;\nlayout(location = 2) uniform uint workCount;\nlayout(location = 3) uniform uint bvhCount;\nlayout(std430, binding = 4) readonly buffer pixBuffer { Pix pixels[]; };\nlayout(std430, binding = 5) readonly buffer meshPBuffer { vec3 positions[]; };\nlayout(std430, binding = 6) readonly buffer bvhBuffer { BVH bvhs[]; };\nlayout(std430, binding = 7) writeonly buffer rCoordBuffer { vec4 r_coords[]; };\nlayout(std430, binding = 8) writeonly buffer rTidxBuffer { uint r_tidx[]; };\nfloat RayAABB(vec3 o, vec3 d, vec3 mins, vec3 maxs)\n{\nvec3 dabs = abs(d);\nvec3 t1 = (mins - o) / d;\nvec3 t2 = (maxs - o) / d;\nvec3 tmin = min(t1, t2);\nvec3 tmax = max(t1, t2);\nfloat a = max(tmin.x, max(tmin.y, tmin.z));\nfloat b = min(tmax.x, min(tmax.y, tmax.z));\nreturn (b >= 0 && a <= b) ? a : FLT_MAX;\n}\nvec3 barycentric(dvec3 p, dvec3 a, dvec3 b, dvec3 c)\n{\ndvec3 v0 = b - a;\ndvec3 v1 = c - a;\ndvec3 v2 = p - a;\ndouble d00 = dot(v0, v0);\ndouble d01 = dot(v0, v1);\ndouble d11 = dot(v1, v1);\ndouble d20 = dot(v2, v0);\ndouble d21 = dot(v2, v1);\ndouble denom = d00 * d11 - d01 * d01;\ndouble y = (d11 * d20 - d01 * d21) / denom;\ndouble z = (d00 * d21 - d01 * d20) / denom;\nreturn vec3(dvec3(1.0 - y - z, y, z));\n}\nvec4 raycast(vec3 o, vec3 d, vec3 a, vec3 b, vec3 c, float mindist, float maxdist)\n{\nvec3 n = normalize(cross(b - a, c - a));\nfloat nd = dot(d, n);\nif (nd > 0)\n{\nfloat pn = dot(o, n);\nfloat t = (dot(a, n) - pn) / nd;\nif (t >= mindist && t < maxdist)\n{\nvec3 p = o + d * t;\nvec3 b = barycentric(p, a, b, c);\nif (b.x >= BARY_MIN && b.y >= BARY_MIN && b.y <= BARY_MAX && b.z >= BARY_MIN && b.z <= BARY_MAX)\n{\nreturn vec4(t, b.x, b.y, b.z);\n}\n}\n}\nreturn vec4(FLT_MAX, 0, 0, 0);\n}\nvoid raycastRange(vec3 o, vec3 d, uint start, uint end, float mindist, in out float curdist, in out uint o_idx, in out vec3 o_bcoord)\n{\nfor (uint tidx = start; tidx < end; tidx += 3)\n{\nvec3 v0 = positions[tidx + 0];\nvec3 v1 = positions[tidx + 1];\nvec3 v2 = positions[tidx + 2];\nvec4 r = raycast(o, d, v0, v1, v2, mindist, curdist);\nif (r.x != FLT_MAX)\n{\ncurdist = r.x;\no_idx = tidx;\no_bcoord = r.yzw;\n}\n}\n}\nvoid raycastBVH(vec3 o, vec3 d, in out float curdist, in out uint o_idx, in out vec3 o_bcoord)\n{\nuint i = 0;\nwhile (i < bvhCount)\n{\nBVH bvh = bvhs[i];\nvec3 aabbMin = vec3(bvh.aabbMinX, bvh.aabbMinY, bvh.aabbMinZ);\nvec3 aabbMax = vec3(bvh.aabbMaxX, bvh.aabbMaxY, bvh.aabbMaxZ);\nfloat distAABB = RayAABB(o, d, aabbMin, aabbMax);\nif (distAABB < curdist)\n{\nraycastRange(o, d, bvh.start, bvh.end, 0, curdist, o_idx, o_bcoord);\n++i;\n}\nelse\n{\ni = bvh.jump;\n}\n}\n}\nvec4 raycastBack(vec3 o, vec3 d, vec3 a, vec3 b, vec3 c, float mindist, float maxdist)\n{\nvec3 n = normalize(cross(b - a, c - a));\nfloat nd = dot(d, n);\nif (nd < 0)\n{\nfloat pn = dot(o, n);\nfloat t = (dot(a, n) - pn) / nd;\nif (t >= mindist && t < maxdist)\n{\nvec3 p = o + d * t;\nvec3 b = barycentric(p, a, b, c);\nif (b.x >= BARY_MIN && b.y >= BARY_MIN && b.y <= BARY_MAX && b.z >= BARY_MIN && b.z <= BARY_MAX)\n{\nreturn vec4(t, b.x, b.y, b.z);\n}\n}\n}\nreturn vec4(FLT_MAX, 0, 0, 0);\n}\nvoid raycastBackRange(vec3 o, vec3 d, uint start, uint end, float mindist, in out float curdist, in out uint o_idx, in out vec3 o_bcoord)\n{\nfor (uint tidx = start; tidx < end; tidx += 3)\n{\nvec3 v0 = positions[tidx + 0];\nvec3 v1 = positions[tidx + 1];\nvec3 v2 = positions[tidx + 2];\nvec4 r = raycastBack(o, d, v0, v1, v2, mindist, curdist);\nif (r.x != FLT_MAX)\n{\ncurdist = r.x;\no_idx = tidx;\no_bcoord = r.yzw;\n}\n}\n}\nvoid raycastBackBVH(vec3 o, vec3 d, in out float curdist, in out uint o_idx, in out vec3 o_bcoord)\n{\nuint i = 0;\nwhile (i < bvhCount)\n{\nBVH bvh = bvhs[i];\nvec3 aabbMin = vec3(bvh.aabbMinX, bvh.aabbMinY, bvh.aabbMinZ);\nvec3 aabbMax = vec3(bvh.aabbMaxX, bvh.aabbMaxY, bvh.aabbMaxZ);\nfloat distAABB = RayAABB(o, d, aabbMin, aabbMax);\nif (distAABB < curdist)\n{\nraycastBackRange(o, d, bvh.start, bvh.end, 0, curdist, o_idx, o_bcoord);\n++i;\n}\nelse\n{\ni = bvh.jump;\n}\n}\n}\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x + workOffset;\nif (gid >= workCount) return;\nPix pix = pixels[gid];\nvec3 p = pix.p;\nvec3 d = pix.d;\nuint tidx = 4294967295;\nvec3 bcoord = vec3(0, 0, 0);\nfloat t = FLT_MAX;\nraycastBVH(p, d, t, tidx, bcoord);\nraycastBackBVH(p, -d, t, tidx, bcoord);\nr_coords[gid] = vec4(t, bcoord.x, bcoord.y, bcoord.z);\nr_tidx[gid] = tidx;\n}\n";
const char normals_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\nlayout(location = 1) uniform uint workOffset;\nlayout(std430, binding = 2) readonly buffer meshNBuffer { vec3 normals[]; };\nlayout(std430, binding = 3) readonly buffer coordsBuffer { vec4 coords[]; };\nlayout(std430, binding = 4) readonly buffer coordsTidxBuffer { uint coords_tidx[]; };\nlayout(std430, binding = 5) writeonly buffer resultBuffer { float results[]; };\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x + workOffset;\nvec4 coord = coords[gid];\nuint tidx = coords_tidx[gid];\nvec3 n0 = normals[tidx + 0];\nvec3 n1 = normals[tidx + 1];\nvec3 n2 = normals[tidx + 2];\nvec3 normal = normalize(coord.y * n0 + coord.z * n1 + coord.w * n2);\nuint ridx = gid * 3;\nresults[ridx + 0] = normal.x;\nresults[ridx + 1] = normal.y;\nresults[ridx + 2] = normal.z;\n}\n";
const char positions_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\n#define FLT_MAX 3.402823466e+38\n#define TANGENT_SPACE 0\nlayout(location = 1) uniform uint workOffset;\nlayout(std430, binding = 2) readonly buffer meshPBuffer { vec3 positions[]; };\nlayout(std430, binding = 3) readonly buffer coordsBuffer { vec4 coords[]; };\nlayout(std430, binding = 4) readonly buffer coordsTidxBuffer { uint coords_tidx[]; };\nlayout(std430, binding = 5) writeonly buffer resultBuffer { float results[]; };\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x + workOffset;\nvec4 coord = coords[gid];\nuint tidx = coords_tidx[gid];\nvec3 p0 = positions[tidx + 0];\nvec3 p1 = positions[tidx + 1];\nvec3 p2 = positions[tidx + 2];\nvec3 p = coord.y * p0 + coord.z * p1 + coord.w * p2;\nuint ridx = gid * 3;\nresults[ridx + 0] = p.x;\nresults[ridx + 1] = p.y;\nresults[ridx + 2] = p.z;\n}\n";
const char tangentspace_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\n#define TANGENT_SPACE 1\nstruct PixelT\n{\nvec3 n;\nvec3 t;\nvec3 b;\n};\nstruct V3 { float x; float y; float z; };\nlayout(location = 1) uniform uint workOffset;\nlayout(std430, binding = 2) readonly buffer pixtBuffer { PixelT pixelst[]; };\nlayout(std430, binding = 3) buffer resultBuffer { V3 results[]; };\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x;\nuint result_idx = gid + workOffset;\nvec3 normal = vec3(results[result_idx].x, results[result_idx].y, results[result_idx].z);\nPixelT pixt = pixelst[result_idx];\nvec3 n = pixt.n;\nvec3 t = pixt.t;\nvec3 b = pixt.b;\nvec3 d0 = vec3(n.z*b.y - n.y*b.z, n.x*b.z - n.z*b.x, n.y*b.x - n.x*b.y);\nvec3 d1 = vec3(t.z*n.y - t.y*n.z, t.x*n.z - n.x*t.z, n.x*t.y - t.x*n.y);\nvec3 d2 = vec3(t.y*b.z - t.z*b.y, t.z*b.x - t.x*b.z, t.x*b.y - t.y*b.x);\nnormal = normalize(vec3(dot(normal, d0), dot(normal, d1), dot(normal, d2)));\nresults[result_idx].x = normal.x;\nresults[result_idx].y = normal.y;\nresults[result_idx].z = normal.z;\n}\n";
const char thick_step1_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\n#define FLT_MAX 3.402823466e+38\nstruct Params\n{\nuint sampleCount;  \nuint samplePermCount;\nfloat minDistance;\nfloat maxDistance;\n};\nstruct BVH\n{\nfloat aabbMinX; float aabbMinY; float aabbMinZ;\nfloat aabbMaxX; float aabbMaxY; float aabbMaxZ;\nuint start;\nuint end;\nuint jump;  \n};\nstruct Input\n{\nvec3 o;\nvec3 d;\nvec3 tx;\nvec3 ty;\n};\nlayout(location = 1) uniform uint pixOffset;\nlayout(location = 2) uniform uint bvhCount;\nlayout(std430, binding = 3) readonly buffer paramsBuffer { Params params; };\nlayout(std430, binding = 4) readonly buffer meshPBuffer { vec3 positions[]; };\nlayout(std430, binding = 5) readonly buffer bvhBuffer { BVH bvhs[]; };\nlayout(std430, binding = 6) readonly buffer samplesBuffer { vec3 samples[]; };\nlayout(std430, binding = 7) readonly buffer inputsBuffer { Input inputs[]; };\nlayout(std430, binding = 8) writeonly buffer resultAccBuffer { float results[]; };\nfloat RayAABB(vec3 o, vec3 d, vec3 mins, vec3 maxs)\n{\nvec3 t1 = (mins - o) / d;\nvec3 t2 = (maxs - o) / d;\nvec3 tmin = min(t1, t2);\nvec3 tmax = max(t1, t2);\nfloat a = max(tmin.x, max(tmin.y, tmin.z));\nfloat b = min(tmax.x, min(tmax.y, tmax.z));\nreturn (b >= 0 && a <= b) ? a : FLT_MAX;\n}\nvec3 barycentric(vec3 p, vec3 a, vec3 b, vec3 c)\n{\nvec3 v0 = b - a;\nvec3 v1 = c - a;\nvec3 v2 = p - a;\nfloat d00 = dot(v0, v0);\nfloat d01 = dot(v0, v1);\nfloat d11 = dot(v1, v1);\nfloat d20 = dot(v2, v0);\nfloat d21 = dot(v2, v1);\nfloat denom = d00 * d11 - d01 * d01;\nfloat y = (d11 * d20 - d01 * d21) / denom;\nfloat z = (d00 * d21 - d01 * d20) / denom;\nreturn vec3(1.0 - y - z, y, z);\n}\n \nfloat raycast(vec3 o, vec3 d, vec3 a, vec3 b, vec3 c)\n{\nvec3 n = normalize(cross(b - a, c - a));\nfloat nd = dot(d, n);\nif (abs(nd) > 0)\n{\nfloat pn = dot(o, n);\nfloat t = (dot(a, n) - pn) / nd;\nif (t >= 0)\n{\nvec3 p = o + d * t;\nvec3 b = barycentric(p, a, b, c);\nif (b.x >= 0 &&  \nb.y >= 0 && b.y <= 1 &&\nb.z >= 0 && b.z <= 1)\n{\nreturn t;\n}\n}\n}\nreturn FLT_MAX;\n}\nfloat raycastRange(vec3 o, vec3 d, uint start, uint end, float mindist)\n{\nfloat mint = FLT_MAX;\nfor (uint tidx = start; tidx < end; tidx += 3)\n{\nvec3 v0 = positions[tidx + 0];\nvec3 v1 = positions[tidx + 1];\nvec3 v2 = positions[tidx + 2];\nfloat t = raycast(o, d, v0, v1, v2);\nif (t >= mindist && t < mint)\n{\nmint = t;\n}\n}\nreturn mint;\n}\nfloat raycastBVH(vec3 o, vec3 d, float mindist, float maxdist)\n{\nfloat mint = FLT_MAX;\nuint i = 0;\nwhile (i < bvhCount)\n{\nBVH bvh = bvhs[i];\nvec3 aabbMin = vec3(bvh.aabbMinX, bvh.aabbMinY, bvh.aabbMinZ);\nvec3 aabbMax = vec3(bvh.aabbMaxX, bvh.aabbMaxY, bvh.aabbMaxZ);\nfloat distAABB = RayAABB(o, d, aabbMin, aabbMax);\nif (distAABB < mint && distAABB < maxdist)\n \n{\nfloat t = raycastRange(o, d, bvh.start, bvh.end, mindist);\nif (t < mint)\n{\nmint = t;\n}\n++i;\n}\nelse\n{\ni = bvh.jump;\n}\n}\nreturn mint;\n}\nvoid main()\n{\nuint in_idx = gl_GlobalInvocationID.x / params.sampleCount;\nuint pix_idx = in_idx + pixOffset;\nuint sample_idx = gl_GlobalInvocationID.x % params.sampleCount;\nuint out_idx = gl_GlobalInvocationID.x;\nInput idata = inputs[in_idx];\nvec3 o = idata.o;\nvec3 d = -idata.d;\nvec3 tx = idata.tx;\nvec3 ty = idata.ty;\nuint sidx = (pix_idx % params.samplePermCount) * params.sampleCount + sample_idx;\nvec3 rs = samples[sidx];\nvec3 sampleDir = normalize(tx * rs.x + ty * rs.y + d * rs.z);\nfloat t = raycastBVH(o, sampleDir, params.minDistance, params.maxDistance);\nresults[out_idx] = (t != FLT_MAX) ? t : params.maxDistance;\n}\n";
const char thick_step2_comp[] = 
"#version 430 core\n#extension GL_ARB_compute_shader : enable\n#extension GL_ARB_shader_storage_buffer_object : enable\nlayout (local_size_x = 64) in;\nstruct Params\n{\nuint sampleCount;  \nfloat minDistance;\nfloat maxDistance;\n};\nlayout(location = 1) uniform uint workOffset;\nlayout(std430, binding = 2) readonly buffer paramsBuffer { Params params; };\nlayout(std430, binding = 3) readonly buffer dataBuffer { float data[]; };\nlayout(std430, binding = 4) writeonly buffer resultAccBuffer { float results[]; };\nvoid main()\n{\nuint gid = gl_GlobalInvocationID.x;\nuint data_start_idx = gid * params.sampleCount;\nfloat acc = 0;\nfor (uint i = 0; i < params.sampleCount; ++i)\n{\nacc += data[data_start_idx + i];\n}\nuint result_idx = gid + workOffset;\nresults[result_idx] = acc / float(params.sampleCount);\n}\n";
