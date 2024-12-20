#pragma once
#include <span>
#include <vector>
#include <d3d11.h>

#include "Vertex.h"
#include "ggp_com_pointer.h"
#include "short_numbers.h"

namespace ggp
{
	class Mesh {
	public:
		// no copy
		Mesh(const Mesh&) = delete;
		const Mesh& operator=(const Mesh&) = delete;
		// yes move, comptr should handle the move
		Mesh(Mesh&&) noexcept = default;
		Mesh& operator=(Mesh&&) noexcept = default;

		// NOTE: this is here because emplacing into some containers wasn't working as expected,
		// didn't want to have to deal with it. TODO remove this
		inline Mesh() = default;

		/// <summary>
		/// Construct a mesh by uploading the given vertices and indices to the GPU. the resulting
		/// mesh will store only handles to the GPU memory. it is the caller's job to free vertex
		/// and index buffers cpu-side
		/// </summary>
		/// <param name="verts">The array of vertices to upload</param>
		/// <param name="indices">Index array. indices should start from the beginning of the given vertex array at 0</param>
		Mesh(std::span<Vertex> verts, std::span<u32> indices) noexcept;

		void BindBuffersAndDraw() const noexcept;

		/// <summary>
		/// Identical to the equivalent constructor but with explicit name to show gpu transfer is happening
		/// </summary>
		inline static Mesh UploadToGPU(std::span<Vertex> verts, std::span<u32> indices) noexcept { return Mesh(verts, indices); }

		using unique_vertices_t = std::vector<Vertex>;
		using unique_indices_t = std::vector<u32>;
		static std::tuple<unique_vertices_t, unique_indices_t> ReadOBJ(const wchar_t* filename) noexcept;

		inline com_p<ID3D11Buffer> GetVertexBuffer() const noexcept { return m_vertexBuffer; }
		inline com_p<ID3D11Buffer> GetIndexBuffer() const noexcept { return m_indexBuffer; }
		inline u64 GetIndexCount() const noexcept { return m_numIndices; }
		inline u64 GetVertexCount() const noexcept { return m_numVertices; }

	private:
		// process some vertices in-place and return the same reference passed in
		static std::span<Vertex> AddTangents(std::span<Vertex> verts, std::span<const u32> indices) noexcept;

		// initialize a vertex buffer handle
		com_p<ID3D11Buffer> UploadVertexBuffer(std::span<Vertex> verts) noexcept;
		// initialize an index buffer handle
		com_p<ID3D11Buffer> UploadIndexBuffer(std::span<u32> indices) noexcept;

		com_p<ID3D11Buffer> m_vertexBuffer;
		com_p<ID3D11Buffer> m_indexBuffer;
		u64 m_numVertices = 0;
		u64 m_numIndices = 0;
	};
}