import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, from } from 'rxjs';
import { map, mergeMap, toArray } from 'rxjs/operators';
import { eASICModel } from 'src/models/enum/eASICModel';

interface GithubRelease {
  id: number;
  tag_name: string;
  name: string;
  prerelease: boolean;
}

interface ASICModelConfig {
  supported_asic_models: eASICModel[];
}

@Injectable({
  providedIn: 'root'
})
export class GithubUpdateService {
  constructor(
    private httpClient: HttpClient
  ) {}

  public getReleases(currentASICModel: eASICModel): Observable<GithubRelease[]> {
    return this.httpClient.get<GithubRelease[]>(
      'https://api.github.com/repos/wantclue/esp-miner-wantclue/releases'
    ).pipe(
      mergeMap((releases: GithubRelease[]) => from(releases)),
      mergeMap((release: GithubRelease) => 
        this.httpClient.get<ASICModelConfig>(
          `https://raw.githubusercontent.com/wantclue/esp-miner-wantclue/${release.tag_name}/asic_models.json`
        ).pipe(
          map(config => ({ release, supported_asic_models: config.supported_asic_models }))
        )
      ),
      toArray(),
      map((releasesWithConfig) => 
        releasesWithConfig.filter(({ release, supported_asic_models }) => 
          !release.prerelease && supported_asic_models.includes(currentASICModel)
        ).map(({ release }) => release)
      )
    );
  }
}