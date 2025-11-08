#if UNITY_EDITOR

using System;
using UnityEngine;

[RequireComponent(typeof(Camera))]
[ExecuteInEditMode]
public class GetExternalMemoryFD : MonoBehaviour
{
    [NonSerialized]
    public RenderTexture RenderTexture;
    [NonSerialized]
    public RenderTexture CameraRenderTexture;

    [ContextMenu("Init")]
    void Init()
    {
        if (RenderTexture != null) { return; }

        RenderTexture = new RenderTexture(1024, 1024, 0, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm);
        CameraRenderTexture = new RenderTexture(1024, 1024, 32, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_UNorm);
        GetComponent<Camera>().targetTexture = CameraRenderTexture;

        ExportTextureInitialize();
    }

    void OnPostRender()
    {
        if (RenderTexture == null) { return; }
        Graphics.CopyTexture(CameraRenderTexture, RenderTexture);
        CopyToExportedTexture(RenderTexture);
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


    // TODO
    //[DllImport( ... )]
    void ExportTextureInitialize() { }

    // TODO
    //[DllImport( ... )]
    void DisposeExportTexture() { }

    // TODO
    //[DllImport( ... )]
    int GetExportFileDescriptor() { return -1; }

    // TODO
    //[DllImport( ... )]
    int ThisPID() { return -1; }
    unsafe void CopyToExportedTexture(RenderTexture renderTexture)
    {
        CopyToExportedTextureImpl((void*)renderTexture.GetNativeTexturePtr());
    }
    // TODO
    //[DllImport( ... )]
    unsafe void CopyToExportedTextureImpl(void* rtNativePtr) { }
}
#endif

