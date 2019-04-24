public enum Color {
  RED(false),
  BLUE(false),
  GREEN(false),
  WHITE(true),
  GREY(true),
  BLACK(true);

  private boolean grayscale;

  Color(boolean grayscale) {
    this.grayscale = grayscale;
  }

  // #CORE
  public boolean isGrayscale() {
    return grayscale;
  }

  // #CORE
  public static Color testStaticInitOfEnum(Color c) {
    if (c.isGrayscale()) {
      return c;
    }
    return c;
  }

  // enums
  public static Color enumField = Color.WHITE;
  // #CORE
  public static Color testEnum() {
    return enumField;
  }

  // #CORE
  public static int testEnumNondet(Color c) {
    if (c == null) return 0;
    if (c == enumField) return 1;
    return 2;
  }

}
