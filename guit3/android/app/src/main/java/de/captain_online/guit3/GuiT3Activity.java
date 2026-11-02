package de.captain_online.guit3;

import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;

import de.captain.cge.CgeActivity;

/**
 * guit3 (libguit3.so) in the engine's activity.
 *
 * The on-screen keyboard covers the bottom of the window, where guit3 has its
 * command line and its status line; the window keeps its size
 * (CgeActivity), so guit3 keeps them above the keyboard itself. It asks
 * getKeyboardInset() for the height the keyboard covers (guiengine.cpp's
 * gui_keyboard_inset()).
 */
public class GuiT3Activity extends CgeActivity {
  /** the height in pixels the keyboard covers, set on the UI thread */
  private volatile int keyboardInset;

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
}
