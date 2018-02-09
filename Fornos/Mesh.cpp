#include "Mesh.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <iterator>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace
{
	enum class WavefrontWarning
	{
		UnknownToken = (1 << 0),
		NotImplemented = (1 << 1),
	};

	enum class WavefrontError
	{
		Syntax = (1 << 0),
		InvalidStatement = (1 << 1),
	};

	enum class WavefrontToken
	{
		Unknown,
		Comment,
		Vertex,
		VertexTexture,
		VertexNormal,
		VertexParameter,
		PolygonPoint,
		PolygonLine,
		PolygonFace,
	};

	WavefrontToken str2token(const std::string::const_iterator begin, const std::string::const_iterator end)
	{
		const auto length = std::distance(begin, end);

		if (*begin == '#')
		{
			return WavefrontToken::Comment;
		}
		if (*begin == 'v')
		{
			if (length == 1)
			{
				return WavefrontToken::Vertex;
			}
			if (length == 2)
			{
				const char secondChar = *(begin + 1);
				if (secondChar == 't') return WavefrontToken::VertexTexture;
				if (secondChar == 'n') return WavefrontToken::VertexNormal;
				if (secondChar == 'p') return WavefrontToken::VertexParameter;
			}
		}
		if (*begin == 'p')
		{
			if (length == 1) return WavefrontToken::PolygonPoint;
		}
		if (*begin == 'l')
		{
			if (length == 1) return WavefrontToken::PolygonLine;
		}
		if (*begin == 'f')
		{
			if (length == 1) return WavefrontToken::PolygonFace;
		}
		return WavefrontToken::Unknown;
	}

	std::string::const_iterator findFirstNoEmpty(const std::string::const_iterator &begin, const std::string::const_iterator &end)
	{
		return std::find_if(begin, end, std::not1(std::ptr_fun<int, int>(std::isspace)));
	}

	std::string::const_iterator findFirstEmpty(const std::string::const_iterator &begin, const std::string::const_iterator &end)
	{
		return std::find_if(begin, end, std::isspace);
	}

	bool readFloat(const char *&strPtr, float &value, bool required = true)
	{
		char *end;
		errno = 0;
		value = static_cast<float>(std::strtod(strPtr, &end));
		if (required && strPtr == end) return false;
		strPtr = end;
		return errno == 0;
	}

	bool readInt(const char *&strPtr, int &value, bool required = true)
	{
		char *end;
		errno = 0;
		value = static_cast<int>(std::strtol(strPtr, &end, 10));
		if (required && strPtr == end) return false;
		strPtr = end;
		return errno == 0;
	}

	bool consumeCharacter(const char *&strPtr, char c)
	{
		if (*strPtr == c)
		{
			++strPtr;
			return true;
		}
		return false;
	}

	void consumeSpaces(const char *&strPtr)
	{
		while (std::isspace(*strPtr))
		{
			++strPtr;
		}
	}

	bool isEndOfLine(const char *strPtr)
	{
		while (*strPtr)
		{
			if (!std::isspace(*strPtr)) return false;
			++strPtr;
		}
		return true;
	}

	struct WavefrontVertex
	{
		float x, y, z, w;
	};

	struct WavefrontTexcoord
	{
		float u, v, w;
	};

	struct WavefrontNormal
	{
		float i, j, k;
	};

	struct WavefrontFaceVertex
	{
		int vertexIndex;
		int texcoordIndex;
		int normalIndex;
	};

	struct WavefrontFaceEntryPoint
	{
		size_t start;
		size_t vertexCount;
	};

	enum class WavefrontFaceOptions
	{
		HasTexcoord = (1 << 0),
		HasNormal = (1 << 1),
	};

	bool operator <(const WavefrontFaceVertex &lhs, const WavefrontFaceVertex &rhs)
	{
		return std::tie(lhs.vertexIndex, lhs.texcoordIndex, lhs.normalIndex) < std::tie(rhs.vertexIndex, rhs.texcoordIndex, rhs.normalIndex);
	}

	template <typename T, typename E>
	class Bitmask
	{
	public:
		Bitmask() : mask(0) {}
		Bitmask(T m) : mask(m) {}
		Bitmask(E e) : mask(static_cast<T>(e)) {}
		Bitmask operator|(Bitmask b) { return Bitmask(mask | b.mask); }
		Bitmask& operator|=(E e) { mask |= static_cast<T>(e); return *this; }
		Bitmask& operator|=(Bitmask rhs) { mask |= rhs.mask; return *this; }
		bool has(E e) { return mask & static_cast<T>(e); }

	private:
		T mask;
	};

#define DeclareBitmask(T,E) \
	Bitmask<T,E> operator|(E a, E b) { return Bitmask<T,E>(a) | Bitmask<T,E>(b); } \
	using Bitmask_ ## E = Bitmask<T, E>;

	DeclareBitmask(uint32_t, WavefrontFaceOptions)
}

#if DEBUG
#define exit_on_fail(x) if (!(x)) { errors |= static_cast<uint32_t>(WavefrontError::Syntax); return nullptr; }
#else
#define exit_on_fail(x) if (!(x)) { return nullptr; }
#endif


Mesh* Mesh::loadWavefrontObj(const char *path)
{
#if DEBUG
	uint32_t warnings = 0u;
	uint32_t errors = 0u;
#endif

	Bitmask_WavefrontFaceOptions faceOptions;
	bool faceOptionsInitialzed = false;

	std::vector<WavefrontVertex> vertices;
	std::vector<WavefrontTexcoord> texcoords;
	std::vector<WavefrontNormal> normals;
	std::vector<WavefrontFaceEntryPoint> faces;
	std::vector<WavefrontFaceVertex> faceVertices;

	std::ifstream ifs;
	ifs.open(path, std::ifstream::in);

	std::string line;

	while (std::getline(ifs, line))
	{
		if (line.size() == 0) continue; // Empty line

		const auto firstTokenBegin = findFirstNoEmpty(line.begin(), line.end());
		const auto firstTokenEnd = findFirstEmpty(firstTokenBegin, line.end());

		if (firstTokenBegin == line.end()) continue; // Empty line

		const WavefrontToken token = str2token(firstTokenBegin, firstTokenEnd);
		switch (token)
		{
		case WavefrontToken::Comment:
		{
			continue;
		} break;
		case WavefrontToken::Vertex:
		{
			float x, y, z, w;
			const char *ptr = &(*firstTokenEnd);
			exit_on_fail(readFloat(ptr, x));
			exit_on_fail(readFloat(ptr, y));
			exit_on_fail(readFloat(ptr, z));
			exit_on_fail(readFloat(ptr, w, false));
			exit_on_fail(isEndOfLine(ptr));
			vertices.emplace_back(WavefrontVertex{ x, y, z, w });
		} break;
		case WavefrontToken::VertexTexture:
		{
			float u, v, w;
			const char *ptr = &(*firstTokenEnd);
			exit_on_fail(readFloat(ptr, u));
			exit_on_fail(readFloat(ptr, v));
			exit_on_fail(readFloat(ptr, w, false));
			exit_on_fail(isEndOfLine(ptr));
			texcoords.emplace_back(WavefrontTexcoord{ u, v, w });
		} break;
		case WavefrontToken::VertexNormal:
		{
			float i, j, k;
			const char *ptr = &(*firstTokenEnd);
			exit_on_fail(readFloat(ptr, i));
			exit_on_fail(readFloat(ptr, j));
			exit_on_fail(readFloat(ptr, k));
			exit_on_fail(isEndOfLine(ptr));
			normals.emplace_back(WavefrontNormal{ i, j, k });
		} break;
		case WavefrontToken::PolygonFace:
		{
			WavefrontFaceEntryPoint entryPoint;
			entryPoint.start = faceVertices.size();
			entryPoint.vertexCount = 0;
			const char *ptr = &(*firstTokenEnd);

			while (!isEndOfLine(ptr))
			{
				WavefrontFaceVertex v;
				v.texcoordIndex = 0;
				v.normalIndex = 0;
				exit_on_fail(readInt(ptr, v.vertexIndex));
				if (consumeCharacter(ptr, '/')) exit_on_fail(readInt(ptr, v.texcoordIndex, false));
				if (consumeCharacter(ptr, '/')) exit_on_fail(readInt(ptr, v.normalIndex));

				if (!faceOptionsInitialzed)
				{
					if (v.texcoordIndex != 0) faceOptions |= WavefrontFaceOptions::HasTexcoord;
					if (v.normalIndex != 0) faceOptions |= WavefrontFaceOptions::HasNormal;
					faceOptionsInitialzed = true;
				}
				else
				{
					exit_on_fail((v.texcoordIndex != 0) == faceOptions.has(WavefrontFaceOptions::HasTexcoord));
					exit_on_fail((v.normalIndex != 0) == faceOptions.has(WavefrontFaceOptions::HasNormal));
				}

				++entryPoint.vertexCount;
				faceVertices.emplace_back(v);

				consumeSpaces(ptr);
			}

			if (entryPoint.vertexCount < 3)
			{
#if DEBUG
				errors |= static_cast<uint32_t>(WavefrontError::InvalidStatement);
				return nullptr;
#endif
			}

			faces.emplace_back(entryPoint);
		} break;
		case WavefrontToken::VertexParameter:
		case WavefrontToken::PolygonPoint:
		case WavefrontToken::PolygonLine:
		{
#if DEBUG
			warnings |= static_cast<uint32_t>(WavefrontWarning::NotImplemented);
#endif
		}
		case WavefrontToken::Unknown:
		default:
		{
#if DEBUG
			warnings |= static_cast<uint32_t>(WavefrontWarning::UnknownToken);
#endif
		}
		}
	}

	ifs.close();

#if 0
	// Transform to a useful format
	struct OutputVertex
	{
		float x, y, z;
		float u, v, w;
		float i, j, k;
	};

	std::vector<OutputVertex> verticesOut;
	std::vector<uint32_t> indicesOut;
	std::map<WavefrontFaceVertex, uint32_t> vertexIndices;

	for (const auto &face : faces)
	{
		uint32_t firstVertexIdx = UINT32_MAX;
		uint32_t previousVertexIdx = UINT32_MAX;

		for (size_t i = 0; i < face.vertexCount; ++i)
		{
			uint32_t vertexIdx = UINT32_MAX;
			{
				const auto &v = faceVertices[face.start + i];
				auto it = vertexIndices.find(v);
				if (it == vertexIndices.end())
				{
					const auto p = vertices[v.vertexIndex - 1];
					const auto t = v.texcoordIndex != 0 ? texcoords[v.texcoordIndex - 1] : WavefrontTexcoord{ 0.0f, 0.0f, 0.0f };
					const auto n = v.normalIndex != 0 ? normals[v.normalIndex - 1] : WavefrontNormal{ 0.0f, 0.0f, 0.0f };
					vertexIdx = (uint32_t)verticesOut.size();
					vertexIndices[v] = vertexIdx;
					verticesOut.emplace_back(OutputVertex{ p.x, p.y, p.z, t.u, t.v, t.w, n.i, n.j, n.k });
				}
				else
				{
					vertexIdx = it->second;
				}
			}

			if (firstVertexIdx == UINT32_MAX)
			{
				firstVertexIdx = vertexIdx;
			}
			else if (previousVertexIdx == UINT32_MAX)
			{
				previousVertexIdx = vertexIdx;
			}
			else
			{
				indicesOut.emplace_back(firstVertexIdx);
				indicesOut.emplace_back(previousVertexIdx);
				indicesOut.emplace_back(vertexIdx);
				previousVertexIdx = vertexIdx;
			}
		}
	}

	// Mesh data
	Mesh* mesh = new Mesh();
	mesh->positions.reserve(indicesOut.size());
	mesh->texcoords.reserve(indicesOut.size());
	mesh->normals.reserve(indicesOut.size());
	for (const auto index : indicesOut)
	{
		const auto &vertex = verticesOut[index];
		mesh->positions.push_back(Vector3(vertex.x, vertex.y, vertex.z));
		mesh->texcoords.push_back(Vector2(vertex.u, vertex.v));
		mesh->normals.push_back(Vector3(vertex.i, vertex.j, vertex.k));
	}
#else
	Mesh *mesh = new Mesh();
	
	mesh->positions.reserve(vertices.size());
	for (const auto &v : vertices) mesh->positions.push_back(Vector3(v.x, v.y, v.z));
	mesh->texcoords.reserve(texcoords.size());
	for (const auto &t : texcoords) mesh->texcoords.push_back(Vector2(t.u, t.v));
	mesh->normals.reserve(normals.size());
	for (const auto &n : normals) mesh->normals.push_back(Vector3(n.i, n.j, n.k));

	std::map<WavefrontFaceVertex, uint32_t> vertexIndices; // Shared vertices map
	
	for (const auto &face : faces)
	{
		uint32_t firstVertexIdx = UINT32_MAX;
		uint32_t previousVertexIdx = UINT32_MAX;

		for (size_t i = 0; i < face.vertexCount; ++i)
		{
			uint32_t vertexIdx = UINT32_MAX;
			{
				const auto &v = faceVertices[face.start + i];
				auto it = vertexIndices.find(v);
				if (it == vertexIndices.end())
				{
					const uint32_t pidx = (uint32_t)(v.vertexIndex - 1);
					const uint32_t tidx = v.texcoordIndex != 0 ? (uint32_t)(v.texcoordIndex - 1) : UINT32_MAX;
					const uint32_t nidx = v.normalIndex != 0 ? (uint32_t)(v.normalIndex - 1) : UINT32_MAX;
					vertexIdx = (uint32_t)mesh->vertices.size();
					vertexIndices[v] = vertexIdx;
					mesh->vertices.emplace_back(Mesh::Vertex{ pidx, tidx, nidx });
				}
				else
				{
					vertexIdx = it->second;
				}
			}

			if (firstVertexIdx == UINT32_MAX)
			{
				firstVertexIdx = vertexIdx;
			}
			else if (previousVertexIdx == UINT32_MAX)
			{
				previousVertexIdx = vertexIdx;
			}
			else
			{
				mesh->triangles.emplace_back(Mesh::Triangle{ firstVertexIdx, previousVertexIdx, vertexIdx });
				previousVertexIdx = vertexIdx;
			}
		}
	}

#endif

	return mesh;
}

void Mesh::computeFaceNormals()
{
	normals.clear();

	for (const auto &tri : triangles)
	{
		auto &v0 = vertices[tri.vertexIndex0];
		auto &v1 = vertices[tri.vertexIndex1];
		auto &v2 = vertices[tri.vertexIndex2];
		const Vector3 p0 = positions[v0.positionIndex];
		const Vector3 p1 = positions[v1.positionIndex];
		const Vector3 p2 = positions[v2.positionIndex];
		const Vector3 n = normalize(cross(p1 - p0, p2 - p0));
		const uint32_t nidx = (uint32_t)normals.size();
		normals.emplace_back(n);
		v0.normalIndex = nidx;
		v1.normalIndex = nidx;
		v2.normalIndex = nidx;
	}
}

// TODO: Improve algorithm
// Face weighting?
void Mesh::computeVertexNormals()
{
	normals.clear();
	normals.resize(positions.size());

	for (const auto &tri : triangles)
	{
		auto &v0 = vertices[tri.vertexIndex0];
		auto &v1 = vertices[tri.vertexIndex1];
		auto &v2 = vertices[tri.vertexIndex2];
		const Vector3 p0 = positions[v0.positionIndex];
		const Vector3 p1 = positions[v1.positionIndex];
		const Vector3 p2 = positions[v2.positionIndex];
		const Vector3 n = normalize(cross(p1 - p0, p2 - p0));
		normals[v0.positionIndex] += n;
		normals[v1.positionIndex] += n;
		normals[v2.positionIndex] += n;
	}

	for (auto &n : normals)
	{
		n = normalize(n);
	}

	for (auto &v : vertices)
	{
		v.normalIndex = v.positionIndex;
	}
}

// TODO: Improve algorithm
void Mesh::computeTangentSpace()
{
	tangents.clear();
	bitangents.clear();
	tangents.resize(vertices.size());
	bitangents.resize(vertices.size());

	for (const auto &tri : triangles)
	{
		const auto &v0 = vertices[tri.vertexIndex0];
		const auto &v1 = vertices[tri.vertexIndex1];
		const auto &v2 = vertices[tri.vertexIndex2];
		const Vector3 p0 = positions[v0.positionIndex];
		const Vector3 p1 = positions[v1.positionIndex];
		const Vector3 p2 = positions[v2.positionIndex];
		const Vector2 u0 = texcoords[v0.texcoordIndex];
		const Vector2 u1 = texcoords[v1.texcoordIndex];
		const Vector2 u2 = texcoords[v2.texcoordIndex];

		const Vector3 e0 = p1 - p0;
		const Vector3 e1 = p2 - p0;
		const Vector2 t0 = u1 - u0;
		const Vector2 t1 = u2 - u0;

		const float r = 1.0f / (t0.x * t1.y - t1.x * t0.y);
		const Vector3 sdir = (e0 * t1.y - e1 * t0.y) * r;
		const Vector3 tdir = (e1 * t0.x - e0 * t1.x) * r;

		tangents[tri.vertexIndex0] += sdir;
		tangents[tri.vertexIndex1] += sdir;
		tangents[tri.vertexIndex2] += sdir;
		bitangents[tri.vertexIndex0] += tdir;
		bitangents[tri.vertexIndex1] += tdir;
		bitangents[tri.vertexIndex2] += tdir;
	}

	for (size_t i = 0; i < tangents.size(); ++i)
	{
		const Vector3 n = normals[vertices[i].normalIndex];
		const Vector3 t1 = tangents[i];
		const Vector3 t2 = bitangents[i];
		tangents[i] = normalize(t1 - n * dot(n, t1));
		bitangents[i] = cross(n, tangents[i]);
		if (dot(cross(n, t1), t2) < 0.0f) bitangents[i] = -bitangents[i];
	}
}

bool Mesh::intersect(const Vector3 &o, const Vector3 &d, IntersectResult &o_result) const
{
	bool intersected = false;

	for (size_t tidx = 0; tidx < triangles.size(); ++tidx)
	{
		IntersectResult r;
		if (intersect(o, d, (uint32_t)tidx, r))
		{
			if (!intersected || o_result.distance > r.distance)
			{
				o_result = r;
				intersected = true;
			}
		}
	}

	return intersected;
}


bool Mesh::intersect(const Vector3 &o, const Vector3 &d, const uint32_t tidx, IntersectResult &o_result) const
{
	const float epsilon = 1e-6f;

	const Triangle &tri = triangles[tidx];
	const Vector3 v0 = positions[vertices[tri.vertexIndex0].positionIndex];
	const Vector3 v1 = positions[vertices[tri.vertexIndex1].positionIndex];
	const Vector3 v2 = positions[vertices[tri.vertexIndex2].positionIndex];
	
	// Moller-Trumbore algorithm

	const Vector3 e0 = v1 - v0;
	const Vector3 e1 = v2 - v0;
	const Vector3 h = cross(d, e1);
	const float a = dot(e0, h);

	if (a > -epsilon && a < epsilon) return false;

	const float f = 1.0f / a;
	const Vector3 s = o - v0;
	const float u = f * dot(s, h);
	if (u < 0.0f || u > 1.0f) return false;

	const Vector3 q = cross(s, e0);
	const float v = f * dot(d, q);
	if (v < 0.0f || (u + v) > 1.0f) return false;

	const float t = f * dot(e1, q);
	if (t < epsilon) return false;

	o_result.tidx = tidx;
	o_result.distance = t;
	return true;
}


void Mesh::intersectAll(const Vector3 *origins, const Vector3 *directions, IntersectResult *o_results, size_t count) const
{
	const float epsilon = 1e-6f;

	for (size_t tidx = 0; tidx < triangles.size(); ++tidx)
	{
		const Triangle &tri = triangles[tidx];
		const Vector3 v0 = positions[vertices[tri.vertexIndex0].positionIndex];
		const Vector3 v1 = positions[vertices[tri.vertexIndex1].positionIndex];
		const Vector3 v2 = positions[vertices[tri.vertexIndex2].positionIndex];

		// Moller-Trumbore algorithm

		const Vector3 e0 = v1 - v0;
		const Vector3 e1 = v2 - v0;

		for (size_t i = 0; i < count; ++i)
		{
			const Vector3 d = directions[i];
			const Vector3 o = origins[i];

			const Vector3 h = cross(d, e1);
			const float a = dot(e0, h);

			if (a < -epsilon || a > epsilon)
			{
				const float f = 1.0f / a;
				const Vector3 s = o - v0;
				const float u = f * dot(s, h);
				if (u >= 0.0f && u <= 1.0f)
				{
					const Vector3 q = cross(s, e0);
					const float v = f * dot(d, q);
					if (v >= 0.0f && (u + v) <= 1.0f)
					{
						const float t = f * dot(e1, q);
						if (t >= epsilon)
						{
							o_results[i].tidx = (uint32_t)tidx;
							o_results[i].distance = t;
						}
					}
				}
			}
		}
	}
}