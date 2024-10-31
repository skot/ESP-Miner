import { Pipe, PipeTransform } from '@angular/core';

@Pipe({
  name: 'formatNumbers'
})
export class FormatNumbersPipe implements PipeTransform {
  transform(value: number): string {
    if (value === null || value === undefined) {
      return '0';
    }

    const absValue = Math.abs(value);
    let formattedValue: string;

    if (absValue >= 1e12) {
      formattedValue = (value / 1e12).toFixed(2) + ' T'; // Trillions
    } else if (absValue >= 1e9) {
      formattedValue = (value / 1e9).toFixed(2) + ' B'; // Billions
    } else if (absValue >= 1e6) {
      formattedValue = (value / 1e6).toFixed(2) + ' M'; // Millions
    } else if (absValue >= 1e3) {
      formattedValue = (value / 1e3).toFixed(2) + ' K'; // Thousands
    } else {
      formattedValue = value.toString(); // Less than 1000
    }

    return formattedValue.replace('.', ','); // Replace '.' with ',' for decimal format
  }
}

@Pipe({
    name: 'formatBtcMined'
})
export class FormatBtcMinedPipe implements PipeTransform {
    transform(value: number): string {
        if (value === null || value === undefined) return '0,00'; // Default return for null/undefined values

        const millions = value / 1_000_000_00; // Convert to millions
        
        // Use toLocaleString with custom options for German-style formatting
        return millions.toLocaleString('en-EN', { minimumFractionDigits: 3, maximumFractionDigits: 3 });
    }
}

@Pipe({
    name: 'formatMinutes'
})
export class FormatMinutesPipe implements PipeTransform {

    transform(value: number): string {
        if (!value) return '0 minutes 0 seconds'; // handle null or undefined

        const totalSeconds = Math.round(value * 60); // Convert minutes to seconds
        const minutes = Math.floor(totalSeconds / 60);
        const seconds = totalSeconds % 60;

        return `${minutes} minute${minutes !== 1 ? 's' : ''} ${seconds} second${seconds !== 1 ? 's' : ''}`;
    }

}
