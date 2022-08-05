#include "gltf_loader.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <assert.h>
#include <optional>
#include <vector>

namespace gltf
{

/*
    Why are we using 127 instead of 127.5 ?

    explanation by Arseny Kapoulkine

    this encoding specifically preserves three points exactly
      -1.0
       0.0
       1.0

    but it's done in the unorm space

    "canonical" unorm encoding you might see, (value * 0.5f + 0.5f) * 255.f + 0.5f,
    doesn't preserve 0 exactly since it ends up being 128, and your decode is (v / 255.f) * 2 - 1 = (128 / 255.f) * 2 - 1 = 0.0039
    which is why we use a custom decoding that divides by 127
*/

float unpackNormUint8(uint8_t v) { return v * (1.0f / 127.0f) - 1.0f; }
uint8_t packNormUint8(float v)
{
    v = (v < -1.0f) ? -1.0f : v;
    v = (v > 1.0f) ? 1.0f : v;
    return (uint8_t)(v * 127.0f + 127.5f);
}

struct Vertex
{
    // pos
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;

    // normal
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 1.0f;

    // uv
    float tu;
    float tv;

    // tangent + sign
    uint8_t tx;
    uint8_t ty;
    uint8_t tz;
    uint8_t tw;

    // vcolor
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    // skinning
    uint8_t joint[4];
    uint8_t weight[4];
};

struct Face
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<Face> faces;

    bool hasNormals = false;
    bool hasUVs = false;
    bool hasTangents = false;
    bool hasColors = false;
    bool hasSkinWeights = false;
};

struct DrawCallDesc
{
    size_t numVertices = 0;
    size_t numIndices = 0;
    size_t numTris = 0;

    const cgltf_accessor* ind16 = nullptr;
    const cgltf_accessor* ind32 = nullptr;

    const cgltf_accessor* pos = nullptr;
    const cgltf_accessor* nrm = nullptr;
    const cgltf_accessor* uv = nullptr;
    const cgltf_accessor* tangent = nullptr;

    const cgltf_accessor* color = nullptr;
    const cgltf_accessor* skinWgt = nullptr;
    const cgltf_accessor* skinInd = nullptr;
};

struct BufferReader
{
    const char* data = nullptr;
    size_t stride = 0;
    size_t count = 0;
    const cgltf_accessor* acc = nullptr;
    cgltf_type type = cgltf_type_invalid;
    cgltf_component_type component_type = cgltf_component_type_invalid;
    bool normalized = false;

    bool isValid() const { return (data != nullptr); }

    template <typename T> const T& getByIndex(size_t index) const
    {
        assert(data);
        assert(index < count);
        const char* element = data + index * stride;
        return *(const T*)element;
    }

    float getAsFloat(size_t index, size_t componentIndex = 0) const
    {
        switch (component_type)
        {
        case cgltf_component_type_r_32f:
        {
            const float* data = &getByIndex<float>(index);
            return data[componentIndex];
        }
        break;
        case cgltf_component_type_r_8u:
        {
            assert(normalized);
            const uint8_t* data = &getByIndex<uint8_t>(index);
            return (data[componentIndex] / 255.0f);
        }
        break;
        case cgltf_component_type_r_16u:
        {
            assert(normalized);
            const uint16_t* data = &getByIndex<uint16_t>(index);
            return (data[componentIndex] / 65535.0f);
        }
        break;
        default:
            assert(false);
            return 0.0f;
        }

        assert(false);
        return 0.0f;
    }
};

inline BufferReader getBufferReader(const cgltf_accessor* acc)
{
    if (!acc)
    {
        return BufferReader{};
    }
    assert(acc->buffer_view);
    //assert(acc->buffer_view->type == cgltf_buffer_view_type_indices || acc->buffer_view->type == cgltf_buffer_view_type_vertices);
    assert(acc->buffer_view->buffer);
    assert(acc->buffer_view->buffer->data);
    char* rawBuffer = (char*)acc->buffer_view->buffer->data;
    size_t byteOffset = (acc->offset + acc->buffer_view->offset);
    size_t stride = acc->stride;
    assert(stride > 0);
    char* data = rawBuffer + byteOffset;
    return BufferReader{data, stride, acc->count, acc, acc->type, acc->component_type, (acc->normalized != 0)};
}

inline std::optional<DrawCallDesc> getDrawcallDesc(const cgltf_primitive* dc)
{
    if (dc->targets_count != 0)
    {
        // blend shapes not supported
        return std::optional<DrawCallDesc>{};
    }

    // only indexed triangles supported
    if (dc->type != cgltf_primitive_type_triangles || !dc->indices)
    {
        return std::optional<DrawCallDesc>{};
    }

    // only 32/16-bit indices supported
    if (dc->indices->type != cgltf_type_scalar)
    {
        return std::optional<DrawCallDesc>{};
    }

    size_t numVertices = 0;
    DrawCallDesc desc;

    if (dc->indices->component_type == cgltf_component_type_r_16u)
    {
        desc.ind16 = dc->indices;
    }

    if (dc->indices->component_type == cgltf_component_type_r_32u)
    {
        desc.ind32 = dc->indices;
    }

    // no indices
    if (!desc.ind32 && !desc.ind16)
    {
        return std::optional<DrawCallDesc>{};
    }

    desc.numIndices = dc->indices->count;

    for (size_t attrIndex = 0; attrIndex < dc->attributes_count; attrIndex++)
    {
        const cgltf_attribute* attr = &dc->attributes[attrIndex];
        assert(attr);
        const cgltf_accessor* vertexAttrData = attr->data;
        assert(vertexAttrData);
        if (numVertices == 0)
        {
            numVertices = vertexAttrData->count;
        }
        assert(vertexAttrData->count == numVertices);

        switch (attr->type)
        {
        case cgltf_attribute_type_position:
            assert(!desc.pos);
            if (vertexAttrData->type != cgltf_type_vec3 || vertexAttrData->component_type != cgltf_component_type_r_32f)
            {
                continue;
            }
            desc.pos = vertexAttrData;
            break;
        case cgltf_attribute_type_normal:
            if (vertexAttrData->type != cgltf_type_vec3 || vertexAttrData->component_type != cgltf_component_type_r_32f)
            {
                continue;
            }
            desc.nrm = vertexAttrData;
            break;
        case cgltf_attribute_type_tangent:
            if (vertexAttrData->type != cgltf_type_vec4 || vertexAttrData->component_type != cgltf_component_type_r_32f)
            {
                continue;
            }
            desc.tangent = vertexAttrData;
            break;
        case cgltf_attribute_type_texcoord:
            if (attr->index != 0 || vertexAttrData->type != cgltf_type_vec2 || vertexAttrData->component_type != cgltf_component_type_r_32f)
            {
                continue;
            }
            desc.uv = vertexAttrData;
            break;
        case cgltf_attribute_type_color:
            if (attr->index != 0 || (vertexAttrData->type != cgltf_type_vec3 && vertexAttrData->type != cgltf_type_vec4))
            {
                continue;
            }
            desc.color = vertexAttrData;
            break;
        case cgltf_attribute_type_joints:
            if (attr->index != 0 || vertexAttrData->type != cgltf_type_vec4)
            {
                continue;
            }
            desc.skinInd = vertexAttrData;
            break;
        case cgltf_attribute_type_weights:
            if (attr->index != 0 || vertexAttrData->type != cgltf_type_vec4)
            {
                continue;
            }
            desc.skinWgt = vertexAttrData;
            break;
        }
    }

    // a valid descriptor should at least have vertex positions
    if (!desc.pos)
    {
        return std::optional<DrawCallDesc>{};
    }

    assert((desc.numIndices % 3) == 0);
    size_t numTris = desc.numIndices / 3;
    desc.numTris = numTris;
    desc.numVertices = numVertices;

    return std::optional<DrawCallDesc>{desc};
}

static void loadMesh(const cgltf_node* node, const cgltf_data* data, MeshData& res)
{
    if (node->has_translation)
    {
        printf("t: %3.2f, %3.2f, %3.2f\n", node->translation[0], node->translation[1], node->translation[2]);
    }

    if (node->has_rotation)
    {
        printf("r: %3.2f, %3.2f, %3.2f, %3.2f\n", node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
    }

    if (node->has_scale)
    {
        printf("s: %3.2f, %3.2f, %3.2f\n", node->scale[0], node->scale[1], node->scale[2]);
    }

    if (node->has_matrix)
    {
        printf("%3.2f, %3.2f, %3.2f, %3.2f\n", node->matrix[0], node->matrix[1], node->matrix[2], node->matrix[3]);
        printf("%3.2f, %3.2f, %3.2f, %3.2f\n", node->matrix[4], node->matrix[5], node->matrix[6], node->matrix[7]);
        printf("%3.2f, %3.2f, %3.2f, %3.2f\n", node->matrix[8], node->matrix[9], node->matrix[10], node->matrix[11]);
        printf("%3.2f, %3.2f, %3.2f, %3.2f\n", node->matrix[12], node->matrix[13], node->matrix[14], node->matrix[15]);
    }

    if (node->skin)
    {
        const cgltf_skin* skin = node->skin;
        printf("Has skin\n");
    }

    const cgltf_mesh* mesh = node->mesh;
    size_t numDrawCalls = mesh->primitives_count;
    for (size_t dcIndex = 0; dcIndex < numDrawCalls; dcIndex++)
    {
        const cgltf_primitive* dc = &mesh->primitives[dcIndex];

        std::optional<DrawCallDesc> desc = getDrawcallDesc(dc);
        if (!desc.has_value())
        {
            continue;
        }

        if (desc->ind16)
        {
            // read index buffer (16-bit)
            BufferReader ind = getBufferReader(desc->ind16);
            for (size_t triIndex = 0; triIndex < desc->numTris; triIndex++)
            {
                Face& face = res.faces.emplace_back();
                face.a = ind.getByIndex<uint16_t>(triIndex * 3 + 0);
                face.b = ind.getByIndex<uint16_t>(triIndex * 3 + 0);
                face.c = ind.getByIndex<uint16_t>(triIndex * 3 + 0);
            }
        }
        else
        {
            // read index buffer (32-bit)
            BufferReader ind = getBufferReader(desc->ind32);
            for (size_t triIndex = 0; triIndex < desc->numTris; triIndex++)
            {
                Face& face = res.faces.emplace_back();
                face.a = ind.getByIndex<uint32_t>(triIndex * 3 + 0);
                face.b = ind.getByIndex<uint32_t>(triIndex * 3 + 0);
                face.c = ind.getByIndex<uint32_t>(triIndex * 3 + 0);
            }
        }

        BufferReader pos = getBufferReader(desc->pos);
        BufferReader nrm = getBufferReader(desc->nrm);
        BufferReader uv = getBufferReader(desc->uv);
        BufferReader tangent = getBufferReader(desc->tangent);
        BufferReader color = getBufferReader(desc->color);
        BufferReader joints = getBufferReader(desc->skinInd);
        BufferReader weights = getBufferReader(desc->skinWgt);


        res.hasNormals = nrm.isValid();
        res.hasUVs = uv.isValid();
        res.hasTangents = tangent.isValid();
        res.hasColors = color.isValid();
        res.hasSkinWeights = weights.isValid() && joints.isValid();

        for (size_t vIndex = 0; vIndex < desc->numVertices; vIndex++)
        {
            Vertex& v = res.vertices.emplace_back();

            // read positions
            assert(pos.component_type == cgltf_component_type_r_32f);
            const float* p = &pos.getByIndex<float>(vIndex);
            v.vx = p[0];
            v.vy = p[1];
            v.vz = p[2];

            // read vertex normal
            if (nrm.isValid())
            {
                v.nx = nrm.getAsFloat(vIndex, 0);
                v.ny = nrm.getAsFloat(vIndex, 1);
                v.nz = nrm.getAsFloat(vIndex, 2);
            }

            // read texture coordinates
            if (uv.isValid())
            {
                v.tu = uv.getAsFloat(vIndex, 0);
                v.tv = uv.getAsFloat(vIndex, 1);
            }

            if (tangent.isValid())
            {
                float tx = tangent.getAsFloat(vIndex, 0);
                float ty = tangent.getAsFloat(vIndex, 1);
                float tz = tangent.getAsFloat(vIndex, 2);
                float tw = tangent.getAsFloat(vIndex, 3);

                // tw (bitangent sign) should be -1.0 or 1.0
                assert(fabsf(fabsf(tw) - 1.0f) < 0.00001f);

                v.tx = packNormUint8(tx);
                v.ty = packNormUint8(ty);
                v.tz = packNormUint8(tz);
                v.tw = packNormUint8(tw);
            }

            if (color.isValid())
            {
                float r = color.getAsFloat(vIndex, 0);
                float g = color.getAsFloat(vIndex, 1);
                float b = color.getAsFloat(vIndex, 2);
                float a = 1.0f;
                if (color.type == cgltf_type_vec4)
                {
                    a = color.getAsFloat(vIndex, 3);
                }
                v.r = (uint8_t)(r * 255.0f + 0.5f);
                v.g = (uint8_t)(g * 255.0f + 0.5f);
                v.b = (uint8_t)(b * 255.0f + 0.5f);
                v.a = (uint8_t)(a * 255.0f + 0.5f);
            }

            if (joints.isValid())
            {
                assert(joints.component_type == cgltf_component_type_r_8u || joints.component_type == cgltf_component_type_r_16u);
                assert(!joints.normalized);

                uint16_t i0 = 0;
                uint16_t i1 = 0;
                uint16_t i2 = 0;
                uint16_t i3 = 0;
                if (joints.component_type == cgltf_component_type_r_8u)
                {
                    const uint8_t* data = &joints.getByIndex<uint8_t>(vIndex);
                    i0 = data[0];
                    i1 = data[1];
                    i2 = data[2];
                    i3 = data[3];
                }
                else if (joints.component_type == cgltf_component_type_r_16u)
                {
                    const uint16_t* data = &joints.getByIndex<uint16_t>(vIndex);
                    i0 = data[0];
                    i1 = data[1];
                    i2 = data[2];
                    i3 = data[3];
                } else
                {
                    assert(false);
                }
            }

            if (weights.isValid())
            {
                float w0 = weights.getAsFloat(vIndex, 0);
                float w1 = weights.getAsFloat(vIndex, 1);
                float w2 = weights.getAsFloat(vIndex, 2);
                float w3 = weights.getAsFloat(vIndex, 3);

                v.weight[0] = (uint8_t)(w0 * 255.0f + 0.5f);
                v.weight[1] = (uint8_t)(w1 * 255.0f + 0.5f);
                v.weight[2] = (uint8_t)(w2 * 255.0f + 0.5f);
                v.weight[3] = (uint8_t)(w3 * 255.0f + 0.5f);
            }
        }
    }
}

int loadFile(const char* filePath)
{
    //
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filePath, &data);
    if (result != cgltf_result_success)
    {
        printf("Can't parse glTF file\n");
        cgltf_free(data);
        return 1;
    }

    result = cgltf_load_buffers(&options, data, filePath);
    if (result != cgltf_result_success)
    {
        printf("Can't load glTF buffers\n");
        cgltf_free(data);
        return 2;
    }

    /*
        result = cgltf_validate(data);
        if (result != cgltf_result_success)
        {
            printf("Can't validate glTF file\n");
            cgltf_free(data);
            return 3;
        }
    */

    if (data->meshes_count == 0 || !data->meshes)
    {
        printf("No meshes\n");
        cgltf_free(data);
        return 4;
    }

    for (size_t nodeIndex = 0; nodeIndex < data->nodes_count; nodeIndex++)
    {
        const cgltf_node* node = &data->nodes[nodeIndex];
        if (node->mesh)
        {
            MeshData meshData;
            loadMesh(node, data, meshData);
        }
    }

    cgltf_free(data);
    return 0;
}

} // namespace gltf