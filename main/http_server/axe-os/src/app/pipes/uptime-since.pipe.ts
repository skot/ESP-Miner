import { Pipe, PipeTransform } from '@angular/core';

@Pipe({
  name: 'uptimeSince',
  pure: true
})
export class UptimeSincePipe implements PipeTransform {

  transform(value: any, args?: any): any {
    if (value) {
      value = new Date().getTime() - value * 1000;
      const seconds = Math.floor((+new Date() - +new Date(value)) / 1000);
      const d = Math.floor(seconds / (3600 * 24))
      const h = Math.floor((seconds % (3600 * 24)) / 3600)
      const m = Math.floor((seconds % 3600) / 60)
      const s = Math.floor(seconds % 60)
      
      const dDisplay = d > 0 ? d + ( "d" ) : ""
      const hDisplay = h > 0 ? h + ( "h" ) : ""
      const mDisplay = m > 0 ? m + ( "m" ) : ""
      const sDisplay = s > 0 ? s + ( "s" ) : ""

      return dDisplay + " " + hDisplay + " " + mDisplay + " " + sDisplay
    }
    return value;
  }

}