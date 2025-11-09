#if UNITY_EDITOR

using System;
using System.Net.Http;
using System.Runtime.InteropServices;
using UnityEngine;

[RequireComponent(typeof(Camera))]
[ExecuteInEditMode]
public class GetExternalMemoryFD : MonoBehaviour
{
    [NonSerialized]
    public RenderTexture RenderTexture;
    [NonSerialized]
    public RenderTexture CameraRenderTexture;

    static HttpClient _client = null;

    [ContextMenu("Init")]
    void Init()
    {
        if (RenderTexture != null) { return; }

        var result = ExportTextureInitialize();
        Debug.Log($"Result:{result}");
        if (result < 0) { return; }

        RenderTexture = new RenderTexture(1024, 1024, 0, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm);
        CameraRenderTexture = new RenderTexture(1024, 1024, 32, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm);
        GetComponent<Camera>().targetTexture = CameraRenderTexture;

        var texID = new ExportTextureID() { FileDescriptor = GetExportFileDescriptor(), Pid = ThisPID() };
        Debug.Log($"fd:{texID.FileDescriptor},pid:{texID.Pid}");
        var jsonStr = JsonUtility.ToJson(texID);

        if (_client == null) { _client = new(); }
        _client.PostAsync("http://127.0.0.1:9400/", new StringContent(jsonStr));
    }
    [Serializable]
    public class ExportTextureID
    {
        public int FileDescriptor;
        public int Pid;
    }

    void OnPostRender()
    {
        if (RenderTexture == null) { return; }
        Graphics.CopyTexture(CameraRenderTexture, RenderTexture);
        CopyToExportedTexture(RenderTexture);

        // ... ?
    }

    [ContextMenu("Exit")]
    void OnDestroy()
    {
        if (GetComponent<Camera>().targetTexture != null)
        {
            GetComponent<Camera>().targetTexture = null;
        }
        if (RenderTexture != null)
        {
            DisposeExportTexture();

            DestroyImmediate(RenderTexture);
            DestroyImmediate(CameraRenderTexture);
            RenderTexture = null;
            CameraRenderTexture = null;
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
    unsafe void CopyToExportedTexture(RenderTexture renderTexture)
    {
        CopyToExportedTextureImpl((void*)renderTexture.GetNativeTexturePtr());
    }
    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern unsafe static void CopyToExportedTextureImpl(void* rtNativePtr);


    [DllImport("VulkanGetExternalMemoryFDPlugin.so")]
    extern static int DebugCall();
}
#endif

