/** Surface schemes. The id becomes a `theme-<id>` class and a block in _theme.scss. */
export interface ColorScheme {
  id: string;
  label: string;
  /**
   * Drives the `dark-mode` class, which is what the component styles key off.
   * It is NOT "does the name sound dark": `light` is a lifted navy with white
   * text, so it is dark-mode too. `white` is the only light surface here.
   */
  dark: boolean;
  description: string;
}

export const COLOR_SCHEMES: ColorScheme[] = [
  {
    id: 'midnight',
    label: 'Midnight',
    dark: true,
    description: 'True black, for OLED panels and dark rooms',
  },
  {
    id: 'dark',
    label: 'Dark',
    dark: true,
    description: 'The default deep navy',
  },
  {
    id: 'slate',
    label: 'Slate',
    dark: true,
    description: 'Softer neutral grey, lower contrast',
  },
  {
    id: 'light',
    label: 'Light',
    // Lighter than `dark`, but still a dark surface with white text, so it
    // keeps `dark-mode`. This matches master's `colorScheme !== 'white'` rule.
    dark: true,
    description: 'Lifted navy, still a dark surface',
  },
  {
    id: 'white',
    label: 'White',
    dark: false,
    description: 'Light surface for bright rooms',
  },
];

export const COLOR_SCHEME_CLASSES: string[] = COLOR_SCHEMES.map(s => `theme-${s.id}`);

export const DEFAULT_COLOR_SCHEME = 'dark';

export function isDarkScheme(id: string): boolean {
  return COLOR_SCHEMES.find(s => s.id === id)?.dark ?? true;
}
