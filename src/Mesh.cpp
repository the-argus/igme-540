#include "Mesh.h"
#include "Graphics.h"

using namespace ggp;
template <typename T>
using span = std::span<T>;

Mesh::Mesh(span<Vertex> vertices, span<u32> indices) noexcept :
	m_vertexBuffer(UploadVertexBuffer(vertices)),
	m_indexBuffer(UploadIndexBuffer(indices)),
	m_numIndices(indices.size()),
	m_numVertices(vertices.size())
{
}

// DRAW geometry
// - These steps are generally repeated for EACH object you draw
// - Other Direct3D calls will also be necessary to do more complex things
void Mesh::BindBuffersAndDraw() const noexcept
{
	// Set buffers in the input assembler (IA) stage
	//  - Do this ONCE PER OBJECT, since each object may have different geometry
	//  - For this demo, this step *could* simply be done once during Init()
	//  - However, this needs to be done between EACH DrawIndexed() call
	//     when drawing different geometry, so it's here as an example
	constexpr u64 numBuffers = 1; // if you increase this, increase strides and put vertex buffers in array for IASetVertexBuffers call
	const UINT stride[numBuffers] = { sizeof(Vertex) };
	const UINT offset[numBuffers] = { 0 };
	Graphics::Context->IASetVertexBuffers(0, numBuffers, m_vertexBuffer.GetAddressOf(), stride, offset);
	Graphics::Context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Tell Direct3D to draw
	//  - Begins the rendering pipeline on the GPU
	//  - Do this ONCE PER OBJECT you intend to draw
	//  - This will use all currently set Direct3D resources (shaders, buffers, etc)
	//  - DrawIndexed() uses the currently set INDEX BUFFER to look up corresponding
	//     vertices in the currently set VERTEX BUFFER
	Graphics::Context->DrawIndexed(
		UINT(m_numIndices),
		0,     // Offset to the first index we want to use
		0);    // Offset to add to each index when looking up vertices
}

// Create a VERTEX BUFFER
// - This holds the vertex data of triangles for a single object
// - This buffer is created on the GPU, which is where the data needs to
//    be if we want the GPU to act on it (as in: draw it to the screen)
com_p<ID3D11Buffer> Mesh::UploadVertexBuffer(span<Vertex> verts) noexcept
{
	// First, we need to describe the buffer we want Direct3D to make on the GPU
	//  - Note that this variable is created on the stack since we only need it once
	//  - After the buffer is created, this description variable is unnecessary
	const D3D11_BUFFER_DESC vbd = {
		.ByteWidth = UINT(verts.size_bytes()),
		.BindFlags = D3D11_BIND_VERTEX_BUFFER, // Tells Direct3D this is a vertex buffer
		.CPUAccessFlags = 0,	// Note: We cannot access the data from C++ (this is good)
		.MiscFlags = 0,
		.StructureByteStride = 0,
	};

	// Create the proper struct to hold the initial vertex data
	// - This is how we initially fill the buffer with data
	// - Essentially, we're specifying a pointer to the data to copy
	const D3D11_SUBRESOURCE_DATA initialVertexData = {
		.pSysMem = verts.data(), // pSysMem = Pointer to System Memory
	};

	// Actually create the buffer on the GPU with the initial data
	// - Once we do this, we'll NEVER CHANGE DATA IN THE BUFFER AGAIN
	com_p<ID3D11Buffer> out;
	Graphics::Device->CreateBuffer(&vbd, &initialVertexData, out.GetAddressOf());
	return out;
}

// Create an INDEX BUFFER
// - This holds indices to elements in the vertex buffer
// - This is most useful when vertices are shared among neighboring triangles
// - This buffer is created on the GPU, which is where the data needs to
//    be if we want the GPU to act on it (as in: draw it to the screen)
com_p<ID3D11Buffer> Mesh::UploadIndexBuffer(span<u32> indices) noexcept
{
	// Describe the buffer, as we did above, with two major differences
	//  - Byte Width (3 unsigned integers vs. 3 whole vertices)
	//  - Bind Flag (used as an index buffer instead of a vertex buffer) 
	const D3D11_BUFFER_DESC ibd = {
		.ByteWidth = UINT(indices.size_bytes()),
		.Usage = D3D11_USAGE_IMMUTABLE,	// Will NEVER change
		.BindFlags = D3D11_BIND_INDEX_BUFFER,	// Tells Direct3D this is an index buffer
		.CPUAccessFlags = 0,	// Note: We cannot access the data from C++ (this is good)
		.MiscFlags = 0,
		.StructureByteStride = 0,
	};

	// Specify the initial data for this buffer, similar to above
	const D3D11_SUBRESOURCE_DATA initialIndexData = {
		.pSysMem = indices.data(), // pSysMem = Pointer to System Memory
	};

	// Actually create the buffer with the initial data
	// - Once we do this, we'll NEVER CHANGE THE BUFFER AGAIN
	com_p<ID3D11Buffer> out;
	Graphics::Device->CreateBuffer(&ibd, &initialIndexData, out.GetAddressOf());
	return out;
}
