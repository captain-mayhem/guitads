package de.captain_online.guit3;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.webkit.CookieManager;
import android.webkit.URLUtil;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;
import android.widget.Toast;

import de.captain.cge.CgeActivity;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

/**
 * guit3 (libguit3.so) in the engine's activity.
 *
 * The on-screen keyboard covers the bottom of the window, where guit3 has its
 * command line and its status line; the window keeps its size
 * (CgeActivity), so guit3 keeps them above the keyboard itself. It asks
 * getKeyboardInset() for the height the keyboard covers (guiengine.cpp's
 * gui_keyboard_inset()).
 *
 * A Web UI game's pages are shown in a WebView over guit3 (showWebUi(), from
 * guit3.cpp's launch hook): a browser in front would leave guit3, and the
 * game's server in it, in the background, where the system may freeze or end
 * them. The WebView is in a full-screen dialog, since the NativeActivity's
 * window belongs to the native code and draws no views. The game's downloads
 * (saved games, transcripts) are saved where the user picks
 * (ACTION_CREATE_DOCUMENT), its uploads (restoring) come from the system's
 * file picker, and other sites open in the browser. Back closes the view
 * after asking, which ends the game's input (nativeWebUiClosed()).
 */
public class GuiT3Activity extends CgeActivity {
  private static final int REQUEST_UPLOAD = 101;
  private static final int REQUEST_DOWNLOAD = 102;

  /** the height in pixels the keyboard covers, set on the UI thread */
  private volatile int keyboardInset;

  private Dialog webUi;
  private WebView webView;
  /** the page's file input waiting for the picker */
  private ValueCallback<Uri[]> uploadCallback;
  /** the download waiting for its target */
  private String downloadUrl;
  private String downloadUserAgent;
  /** the game's downloads, by URL, until they are saved */
  private final Map<String, byte[]> downloads = new HashMap<>();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    View decor = getWindow().getDecorView();
    decor.setOnApplyWindowInsetsListener((view, insets) -> {
      keyboardInset = keyboardInset(view, insets);
      return view.onApplyWindowInsets(insets);
    });
    // before Android 11 the insets don't say where the keyboard is: the
    // visible part of the window does
    if (Build.VERSION.SDK_INT < 30) {
      decor.getViewTreeObserver().addOnGlobalLayoutListener(
          () -> keyboardInset = keyboardInset(decor, null));
    }
  }

  @Override
  protected void onDestroy() {
    if (webUi != null)
      webUi.dismiss();
    if (webView != null)
      webView.destroy();
    super.onDestroy();
  }

  private static int keyboardInset(View view, WindowInsets insets) {
    if (Build.VERSION.SDK_INT >= 30)
      return insets.getInsets(WindowInsets.Type.ime()).bottom;
    Rect visible = new Rect();
    view.getWindowVisibleDisplayFrame(visible);
    return Math.max(0, view.getRootView().getHeight() - visible.bottom);
  }

  /** the height in pixels the on-screen keyboard covers at the bottom; called on the engine's thread */
  public int getKeyboardInset() {
    return keyboardInset;
  }

  // the Web UI

  /** the user closed the Web UI's view; registered by guiengine.cpp */
  private native void nativeWebUiClosed();

  /** shows url in the Web UI's view, opening it if it isn't; called on the engine's thread */
  public void showWebUi(String url) {
    runOnUiThread(() -> {
      if (webUi == null)
        createWebUi(Uri.parse(url).getHost());
      webView.loadUrl(url);
      webUi.show();
    });
  }

  private void createWebUi(String host) {
    webView = new WebView(this);
    WebSettings settings = webView.getSettings();
    settings.setJavaScriptEnabled(true);
    settings.setDomStorageEnabled(true);
    // the game plays its sounds when it wants to
    settings.setMediaPlaybackRequiresUserGesture(false);
    webView.setWebViewClient(new WebViewClient() {
      /*
       * The game serves a download (/clienttmp/...) once and then deletes it,
       * and the request that tells WebView it's a download is that once. So
       * the file is fetched here and kept for the download listener, and
       * WebView gets the same reply.
       */
      @Override
      public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
        Uri uri = request.getUrl();
        if (!"GET".equals(request.getMethod()) || uri.getPath() == null
            || !uri.getPath().startsWith("/clienttmp/") || uri.getQueryParameter("cancel") != null)
          return null;
        String url = uri.toString();
        try {
          HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
          String cookies = CookieManager.getInstance().getCookie(url);
          if (cookies != null)
            connection.setRequestProperty("Cookie", cookies);
          for (Map.Entry<String, String> header : request.getRequestHeaders().entrySet())
            connection.setRequestProperty(header.getKey(), header.getValue());
          try {
            int status = connection.getResponseCode();
            InputStream in = status < 400 ? connection.getInputStream() : connection.getErrorStream();
            byte[] data = in != null ? readAll(in) : new byte[0];
            String type = connection.getContentType();
            String mime = type != null ? type.split(";")[0].trim() : "application/octet-stream";
            Map<String, String> headers = new HashMap<>();
            String disposition = connection.getHeaderField("Content-Disposition");
            if (disposition != null)
              headers.put("Content-Disposition", disposition);
            if (status == 200) {
              synchronized (downloads) {
                downloads.put(url, data);
              }
            }
            String reason = connection.getResponseMessage();
            return new WebResourceResponse(mime, null, status, reason != null && !reason.isEmpty() ? reason : "OK",
                headers, new ByteArrayInputStream(data));
          } finally {
            connection.disconnect();
          }
        } catch (IOException | RuntimeException e) {
          // WebView requests it itself then
          return null;
        }
      }

      @Override
      public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
        // the game's pages stay here, other sites open in the browser
        Uri uri = request.getUrl();
        if (host != null && host.equals(uri.getHost()))
          return false;
        try {
          startActivity(new Intent(Intent.ACTION_VIEW, uri));
        } catch (ActivityNotFoundException e) {
          // nothing to open it with
        }
        return true;
      }
    });
    webView.setWebChromeClient(new WebChromeClient() {
      @Override
      public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback,
          FileChooserParams params) {
        if (uploadCallback != null)
          uploadCallback.onReceiveValue(null);
        uploadCallback = callback;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        try {
          startActivityForResult(intent, REQUEST_UPLOAD);
        } catch (ActivityNotFoundException e) {
          uploadCallback = null;
          return false;
        }
        return true;
      }
    });
    webView.setDownloadListener((url, userAgent, disposition, mimeType, length) -> {
      downloadUrl = url;
      downloadUserAgent = userAgent;
      Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
      intent.addCategory(Intent.CATEGORY_OPENABLE);
      intent.setType(mimeType != null && !mimeType.isEmpty() ? mimeType : "application/octet-stream");
      intent.putExtra(Intent.EXTRA_TITLE, URLUtil.guessFileName(url, disposition, mimeType));
      try {
        startActivityForResult(intent, REQUEST_DOWNLOAD);
      } catch (ActivityNotFoundException e) {
        downloadUrl = null;
      }
    });
    // the page keeps out of the system bars and the keyboard: the dialog's
    // window is laid out behind them (edge to edge), and a WebView ignores
    // its own padding, so a frame around it has it
    FrameLayout frame = new FrameLayout(this);
    frame.setBackgroundColor(Color.WHITE);
    frame.addView(webView, new FrameLayout.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    frame.setOnApplyWindowInsetsListener((view, insets) -> {
      if (Build.VERSION.SDK_INT >= 30) {
        android.graphics.Insets bars = insets.getInsets(
            WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout() | WindowInsets.Type.ime());
        view.setPadding(bars.left, bars.top, bars.right, bars.bottom);
      }
      return insets;
    });

    webUi = new Dialog(this, android.R.style.Theme_Material_Light_NoActionBar) {
      @Override
      public void onBackPressed() {
        confirmCloseWebUi();
      }
    };
    webUi.setCancelable(false);
    webUi.setContentView(frame, new ViewGroup.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    if (webUi.getWindow() != null) {
      webUi.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
      webUi.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
    }
  }

  private void confirmCloseWebUi() {
    new AlertDialog.Builder(this)
        .setMessage(R.string.close_webui)
        .setPositiveButton(R.string.close, (dialog, which) -> {
          webUi.dismiss();
          webView.loadUrl("about:blank");
          nativeWebUiClosed();
        })
        .setNegativeButton(R.string.cancel, null)
        .show();
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    super.onActivityResult(requestCode, resultCode, data);
    Uri uri = resultCode == RESULT_OK && data != null ? data.getData() : null;
    if (requestCode == REQUEST_UPLOAD && uploadCallback != null) {
      uploadCallback.onReceiveValue(uri != null ? new Uri[] {uri} : null);
      uploadCallback = null;
    } else if (requestCode == REQUEST_DOWNLOAD && downloadUrl != null) {
      if (uri != null) {
        download(downloadUrl, downloadUserAgent, uri);
      } else {
        synchronized (downloads) {
          downloads.remove(downloadUrl);
        }
      }
      downloadUrl = null;
    }
  }

  /**
   * writes a download to target: the one the game served (kept by
   * shouldInterceptRequest), or else fetched from its server
   */
  private void download(String url, String userAgent, Uri target) {
    byte[] kept;
    synchronized (downloads) {
      kept = downloads.remove(url);
    }
    String cookies = CookieManager.getInstance().getCookie(url);
    new Thread(() -> {
      String error = null;
      try {
        byte[] data = kept;
        if (data == null) {
          HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
          if (cookies != null)
            connection.setRequestProperty("Cookie", cookies);
          if (userAgent != null)
            connection.setRequestProperty("User-Agent", userAgent);
          try (InputStream in = connection.getInputStream()) {
            data = readAll(in);
          } finally {
            connection.disconnect();
          }
        }
        try (OutputStream out = getContentResolver().openOutputStream(target)) {
          if (out == null)
            throw new IOException("can't write " + target);
          out.write(data);
        }
      } catch (IOException | RuntimeException e) {
        error = e.getMessage() != null ? e.getMessage() : e.toString();
      }
      final String failure = error;
      runOnUiThread(() -> Toast.makeText(this, failure == null ? getString(R.string.saved)
          : getString(R.string.save_failed, failure), Toast.LENGTH_LONG).show());
    }).start();
  }

  private static byte[] readAll(InputStream in) throws IOException {
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    byte[] buffer = new byte[64 * 1024];
    for (int read; (read = in.read(buffer)) > 0; )
      out.write(buffer, 0, read);
    return out.toByteArray();
  }
}
