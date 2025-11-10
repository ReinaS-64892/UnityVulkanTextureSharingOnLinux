#if UNITY_EDITOR

using System;
using System.Net.Http;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

[RequireComponent(typeof(Camera))]
[ExecuteAlways]
public class GetExternalMemoryFD : MonoBehaviour
{
    public RenderTexture SharedRenderTexture;
    public RenderTexture CameraRenderTexture;
    private Camera s_Camera;

    [ContextMenu("Init")]
    void Init()
    {
        if (SharedRenderTexture != null) { return; }


        SharedRenderTexture = new RenderTexture(512, 512, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm, UnityEngine.Experimental.Rendering.GraphicsFormat.None, 0);
        CameraRenderTexture = new RenderTexture(512, 512, 32, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm);
        SharedRenderTexture.name = "Shared";
        CameraRenderTexture.name = "Camera";

        s_Camera = GetComponent<Camera>();
        s_Camera.targetTexture = CameraRenderTexture;

        // native ptr を生成する必要がある。
        SharedRenderTexture.Create();
        var result = ExportTexture(SharedRenderTexture);
        Debug.Log($"Result:{result}");
        if (result < 0)
        {
            OnDestroy();
            return;
        }

        var texID = new ExportTextureID() { FileDescriptor = GetExportFileDescriptor(), Pid = ThisPID() };
        Debug.Log($"pid-fd:{texID.Pid} {texID.FileDescriptor}");

        EditorApplication.update -= OnCameraRender;
        EditorApplication.update += OnCameraRender;
    }


    [Serializable]
    public class ExportTextureID
    {
        public int FileDescriptor;
        public int Pid;
    }
    void OnCameraRender()
    {
        if (SharedRenderTexture == null) { return; }
        if (s_Camera == null) { return; }
        s_Camera.Render();
        Graphics.Blit(CameraRenderTexture, SharedRenderTexture);
        // GL.IssuePluginEvent(GetRenderEventFunc(), 1);
        // Debug.Log("sync!");
    }

    [ContextMenu("Exit")]
    void OnDestroy()
    {
        if (s_Camera.targetTexture != null)
        {
            s_Camera.targetTexture = null;
        }
        if (SharedRenderTexture != null)
        {
            DisposeExportTexture();

            DestroyImmediate(SharedRenderTexture);
            DestroyImmediate(CameraRenderTexture);
            SharedRenderTexture = null;
            CameraRenderTexture = null;

            EditorApplication.update -= OnCameraRender;
        }
    }
    [ContextMenu("DebugCall")]
    void DebugCallUnity()
    {
        Debug.Log($"{DebugCall()}");
    }

    // [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    // extern static int ExportTextureInitialize();


    unsafe int ExportTexture(RenderTexture renderTexture)
    {
        return ExportTextureImpl((void*)renderTexture.GetNativeTexturePtr());
    }
    [DllImport("VulkanGetExternalMemoryFDPlugin.so", EntryPoint = "ExportTexture")]
    extern unsafe static int ExportTextureImpl(void* rtNativePtr);

    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static void DisposeExportTexture();


    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static int GetExportFileDescriptor();

    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static int ThisPID();

    // [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    // unsafe extern static void* SharedTextureNativePtr();

    // unsafe int ExportTexture(RenderTexture renderTexture)
    // {
    //     return ExportTextureImpl((void*)renderTexture.GetNativeTexturePtr());
    // }
    // [DllImport("VulkanGetExternalMemoryFDPlugin.so", EntryPoint = "ExportTexture")]
    // extern unsafe static int ExportTextureImpl(void* rtNativePtr);

    // [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    // private static extern IntPtr GetRenderEventFunc();


    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static int DebugCall();
}
#endif

