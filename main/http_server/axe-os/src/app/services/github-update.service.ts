import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { map, tap } from 'rxjs/operators';
import { eASICModel } from 'src/models/enum/eASICModel';

interface GithubRelease {
  id: number;
  tag_name: string;
  name: string;
  body: string;
  prerelease: boolean;
}

interface ASICModelConfig {
  supported_asic_models: string[];
}

interface ReleaseWithConfig {
  release: GithubRelease;
  supported_asic_models: string[];
}

@Injectable({
  providedIn: 'root'
})
export class GithubUpdateService {
  constructor(private httpClient: HttpClient) {}

  public getReleases(currentASICModel: eASICModel): Observable<GithubRelease[]> {
    console.log('Fetching releases for ASIC model:', currentASICModel);
    return this.httpClient.get<GithubRelease[]>(
      'https://api.github.com/repos/Skot/ESP-Miner/releases'
    ).pipe(
      tap(releases => console.log('Fetched releases:', releases)),
      map(releases => releases.map(release => {
        const supportedModels = this.parseSupportedModels(release.body);
        console.log(`Release ${release.tag_name} supported models:`, supportedModels);
        return { ...release, supportedModels };
      })),
      map(releases => releases.filter(release => {
        const isCompatible = !release.prerelease && release.supportedModels.includes(eASICModel[currentASICModel]);
        console.log(`Release ${release.tag_name} compatible: ${isCompatible}`);
        return isCompatible;
      })),
      tap(compatibleReleases => console.log('Compatible releases:', compatibleReleases))
    );
  }

  private parseSupportedModels(description: string): string[] {
    const match = description.match(/supports the following ASIC models: ([\w, ]+)/);
    if (match && match[1]) {
      return match[1].split(', ').map(model => model.trim());
    }
    return [];
  }
}