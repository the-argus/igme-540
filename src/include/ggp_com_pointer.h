#pragma once
#include <wrl/client.h>

namespace ggp
{
	template <class T>
	using com_p = Microsoft::WRL::ComPtr<T>;
}