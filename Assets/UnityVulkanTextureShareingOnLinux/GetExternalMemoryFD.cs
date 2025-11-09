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
    public RenderTexture RenderTexture;
    public RenderTexture CameraRenderTexture;
    private Camera s_Camera;

    [ContextMenu("Init")]
    void Init()
    {
        if (RenderTexture != null) { return; }

        var result = ExportTextureInitialize();
        Debug.Log($"Result:{result}");
        if (result < 0) { return; }

        RenderTexture = new RenderTexture(1024, 1024, 0, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm);
        CameraRenderTexture = new RenderTexture(1024, 1024, 32, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm);
        s_Camera = GetComponent<Camera>();
        s_Camera.targetTexture = CameraRenderTexture;

        var texID = new ExportTextureID() { FileDescriptor = GetExportFileDescriptor(), Pid = ThisPID() };
        Debug.Log($"pid-fd:{texID.Pid} {texID.FileDescriptor}");

        var exportResult = ExportTexture(RenderTexture);
        Debug.Log("export! " + exportResult);

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
        if (RenderTexture == null) { return; }
        if (s_Camera == null) { return; }
        s_Camera.Render();
        Graphics.CopyTexture(CameraRenderTexture, RenderTexture);
        GL.IssuePluginEvent(GetRenderEventFunc(), 1);
        Debug.Log("sync!");
    }

    [ContextMenu("Exit")]
    void OnDestroy()
    {
        if (s_Camera.targetTexture != null)
        {
            s_Camera.targetTexture = null;
        }
        if (RenderTexture != null)
        {
            DisposeExportTexture();

            DestroyImmediate(RenderTexture);
            DestroyImmediate(CameraRenderTexture);
            RenderTexture = null;
            CameraRenderTexture = null;

            EditorApplication.update -= OnCameraRender;
        }
    }
    [ContextMenu("DebugCall")]
    void DebugCallUnity()
    {
        Debug.Log($"{DebugCall()}");
    }

    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static int ExportTextureInitialize();

    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static void DisposeExportTexture();

    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static int GetExportFileDescriptor();

    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static int ThisPID();
    unsafe int ExportTexture(RenderTexture renderTexture)
    {
        return ExportTextureImpl((void*)renderTexture.GetNativeTexturePtr());
    }
    [DllImport("VulkanGetExternalMemoryFDPlugin.so", EntryPoint = "ExportTexture")]
    extern unsafe static int ExportTextureImpl(void* rtNativePtr);

    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    private static extern IntPtr GetRenderEventFunc();

    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static int DebugCall();
}
#endif

