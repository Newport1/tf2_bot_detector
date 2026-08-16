#pragma once
#include <functional>
#include <string>

class TF2BotDetectorRendererBase {
public:
	TF2BotDetectorRendererBase() {
		ptr = this;
	}
	~TF2BotDetectorRendererBase() {
		ptr = nullptr;
	}

	/// <summary>
	/// ImGui::Render() related functions.
	/// </summary>
	virtual void DrawFrame() = 0;

	/// <summary>
	/// registerable draw() function.
	/// </summary>
	typedef std::function<void()> DrawableCallbackFn;

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	virtual std::size_t RegisterDrawCallback(DrawableCallbackFn) = 0;

	/// <summary>
	/// Should we stop running and destroy?
	/// </summary>
	/// <returns></returns>
	virtual bool ShouldQuit() const = 0;

	/// <summary>
	/// Request application destruction.
	/// </summary>
	/// <returns></returns>
	virtual void RequestQuit() = 0;
	
	/// <summary>
	/// how frequent should DrawFrame run?
	/// </summary>
	/// <param name="frameTime">frame time in ms</param>
	virtual void SetFramerate(float) = 0;
	virtual float GetFramerate() const = 0;

	/// <summary>
	/// is this "window" in focus?
	/// </summary>
	/// <returns></returns>
	virtual bool InFocus() const = 0;

	/// <summary>
	/// Short description of the active renderer backend (name + API / GLSL version).
	/// </summary>
	/// <returns></returns>
	virtual std::string RendererInfo() const = 0;


	// do we even need these features?
	inline static TF2BotDetectorRendererBase* ptr;

	/// <summary>
	/// Gets the current renderer.
	///
	/// in case anyone needs to
	/// "TF2BotDetectorRendererBase::GetRenderer()->RegisterDrawCallback([this]() { this->Draw(); });"
	/// or something
	/// </summary>
	/// <returns></returns>
	static TF2BotDetectorRendererBase* GetRenderer() { return ptr; }
};
