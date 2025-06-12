import { LitElement, html, css } from 'lit'
import { customElement, state } from 'lit/decorators.js'

@customElement('yakety-app')
export class YaketyApp extends LitElement {
    createRenderRoot() {
        return this
    }
    @state()
    private message = ''

    @state()
    private loading = false

    private async testAPI() {
        this.loading = true
        this.message = 'Testing API...'

        try {
            const response = await fetch('/api/hello')
            const data = await response.json()
            this.message = `API Response: ${data.message}`
        } catch (error) {
            this.message = `API Error: ${error}`
        } finally {
            this.loading = false
        }
    }

    render() {
        return html`
            <div>
                <h1 class="text-red-500">Yakety AI!</h1>
                <p>Voice-to-text input for any application.</p>
                <p>Hold a hotkey to record, release to transcribe and paste.</p>

                <button @click=${this.testAPI} ?disabled=${this.loading}>
                    ${this.loading ? 'Testing...' : 'Test API'}
                </button>

                ${this.message ? html` <div><strong>Status:</strong> ${this.message}</div> ` : ''}
            </div>
        `
    }
}
