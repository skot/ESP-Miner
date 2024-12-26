import { Injectable, effect, signal } from '@angular/core';
import { Subject } from 'rxjs';
import { LocalStorageService } from '../../local-storage.service';

export interface AppConfig {
    inputStyle: string;
    colorScheme: string;
    theme: string;
    ripple: boolean;
    menuMode: string;
    scale: number;
}

interface LayoutState {
    staticMenuDesktopInactive: boolean;
    overlayMenuActive: boolean;
    profileSidebarVisible: boolean;
    configSidebarVisible: boolean;
    staticMenuMobileActive: boolean;
    menuHoverActive: boolean;
}

@Injectable({
    providedIn: 'root',
})
export class LayoutService {
    private darkTheme = {
        '--surface-a': '#0B1219',  // Darker navy
        '--surface-b': '#070D17',  // Very dark navy (from image)
        '--surface-c': 'rgba(255,255,255,0.03)',
        '--surface-d': '#1A2632',  // Slightly lighter navy
        '--surface-e': '#0B1219',
        '--surface-f': '#0B1219',
        '--surface-ground': '#070D17',
        '--surface-section': '#070D17',
        '--surface-card': '#0B1219',
        '--surface-overlay': '#0B1219',
        '--surface-border': '#1A2632',
        '--surface-hover': 'rgba(255,255,255,0.03)',
        '--text-color': 'rgba(255, 255, 255, 0.87)',
        '--text-color-secondary': 'rgba(255, 255, 255, 0.6)',
        '--maskbg': 'rgba(0,0,0,0.4)'
    };

    private lightTheme = {
        '--surface-a': '#1a2632',  // Lighter navy for main background
        '--surface-b': '#243447',  // Medium navy for secondary background
        '--surface-c': 'rgba(255,255,255,0.03)',
        '--surface-d': '#2f4562',  // Light navy for borders
        '--surface-e': '#1a2632',
        '--surface-f': '#1a2632',
        '--surface-ground': '#243447',
        '--surface-section': '#1a2632',
        '--surface-card': '#1a2632',
        '--surface-overlay': '#1a2632',
        '--surface-border': '#2f4562',
        '--surface-hover': 'rgba(255,255,255,0.03)',
        '--text-color': 'rgba(255, 255, 255, 0.9)',  // Slightly brighter text
        '--text-color-secondary': 'rgba(255, 255, 255, 0.7)',  // Brighter secondary text
        '--maskbg': 'rgba(0,0,0,0.2)'
    };

    _config: AppConfig = {
        ripple: false,
        inputStyle: 'outlined',
        menuMode: 'static',
        colorScheme: 'dark',
        theme: 'lara-dark-indigo',
        scale: 14,
    };

    config = signal<AppConfig>(this._config);

    state: LayoutState = {
        staticMenuDesktopInactive: false,
        overlayMenuActive: false,
        profileSidebarVisible: false,
        configSidebarVisible: false,
        staticMenuMobileActive: false,
        menuHoverActive: false,
    };

    private configUpdate = new Subject<AppConfig>();

    private overlayOpen = new Subject<any>();

    configUpdate$ = this.configUpdate.asObservable();

    overlayOpen$ = this.overlayOpen.asObservable();

    constructor(private localStorage: LocalStorageService) {
        // Load saved theme settings or use dark theme as default
        const savedConfig = this.localStorage.getObject('theme-config');
        if (savedConfig) {
            this._config = { ...this._config, ...savedConfig };
        } else {
            // Ensure dark theme is saved as default
            this.localStorage.setObject('theme-config', {
                colorScheme: 'dark',
                theme: 'lara-dark-indigo'
            });
        }

        // Update signal with initial config
        this.config.set(this._config);

        // Apply initial theme
        this.changeTheme();

        effect(() => {
            const config = this.config();
            this.changeTheme();
            this.changeScale(config.scale);
            this.onConfigUpdate();
        });
    }

    onMenuToggle() {
        if (this.isOverlay()) {
            this.state.overlayMenuActive = !this.state.overlayMenuActive;
            if (this.state.overlayMenuActive) {
                this.overlayOpen.next(null);
            }
        }

        if (this.isDesktop()) {
            this.state.staticMenuDesktopInactive =
                !this.state.staticMenuDesktopInactive;
        } else {
            this.state.staticMenuMobileActive =
                !this.state.staticMenuMobileActive;

            if (this.state.staticMenuMobileActive) {
                this.overlayOpen.next(null);
            }
        }
    }

    showProfileSidebar() {
        this.state.profileSidebarVisible = !this.state.profileSidebarVisible;
        if (this.state.profileSidebarVisible) {
            this.overlayOpen.next(null);
        }
    }

    showConfigSidebar() {
        this.state.configSidebarVisible = true;
    }

    isOverlay() {
        return this.config().menuMode === 'overlay';
    }

    isDesktop() {
        return window.innerWidth > 991;
    }

    isMobile() {
        return !this.isDesktop();
    }

    onConfigUpdate() {
        this._config = { ...this.config() };
        this.configUpdate.next(this.config());
        // Save theme settings
        this.localStorage.setObject('theme-config', {
            colorScheme: this._config.colorScheme,
            theme: this._config.theme
        });
        // Apply theme changes immediately
        this.changeTheme();
    }

    changeTheme() {
        const config = this.config();

        // Apply light/dark theme variables
        const themeVars = config.colorScheme === 'light' ? this.lightTheme : this.darkTheme;
        Object.entries(themeVars).forEach(([key, value]) => {
            document.documentElement.style.setProperty(key, value);
        });

        // Load saved accent colors
        const savedTheme = this.localStorage.getObject('theme-colors');
        if (savedTheme && savedTheme.accentColors) {
            Object.entries(savedTheme.accentColors).forEach(([key, value]) => {
                document.documentElement.style.setProperty(key, value as string);
            });
        }
    }

    changeScale(value: number) {
        document.documentElement.style.fontSize = `${value}px`;
    }
}
